/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2007 Peter Kelly (pmk@cs.adelaide.edu.au)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * $Id$
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "src/nreduce.h"
#include "compiler/source.h"
#include "runtime/runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

#define TRACE_PREFIX "reduction"

static void dot_edge(FILE *f, pntr from, pntr to, int doind, const char *attrs)
{
  char fromstr[50];
  char tostr[50];

  if (!doind) {
    from = resolve_pntr(from);
    to = resolve_pntr(to);
  }

  snprintf(fromstr,50,"c%08x%08x",((int*)&from)[0],((int*)&from)[1]);
  snprintf(tostr,50,"c%08x%08x;\n",((int*)&to)[0],((int*)&to)[1]);

  if ((CELL_NUMBER == pntrtype(to)) || (CELL_NIL == pntrtype(to)))
    snprintf(tostr,50,"p%08x%08xc%08x%08x",
            ((int*)&from)[0],((int*)&from)[1],((int*)&to)[0],((int*)&to)[1]);
  else
    snprintf(tostr,50,"c%08x%08x\n",((int*)&to)[0],((int*)&to)[1]);

  if (attrs)
    fprintf(f,"%s -> %s [%s];\n",fromstr,tostr,attrs);
  else
    fprintf(f,"%s -> %s;\n",fromstr,tostr);
}

static void dot_graph_r(FILE *f, pntr p, pntrset *done, int doind, dotfun fun, pntr arg,
                        pntr parent, int landscape)
{
  cell *c;

  if (!doind)
    p = resolve_pntr(p);

  if (CELL_NUMBER == pntrtype(p)) {
    if (!is_nullpntr(parent))
      fprintf(f,"p%08x%08x",((int*)&parent)[0],((int*)&parent)[1]);
    fprintf(f,"c%08x%08x [",((int*)&p)[0],((int*)&p)[1]);
    if (fun)
      fun(f,p,arg);
    fprintf(f,"label=\"%d\"];\n",(int)pntrdouble(p));
    return;
  }
  else if (CELL_NIL == pntrtype(p)) {
    if (!is_nullpntr(parent))
      fprintf(f,"p%08x%08x",((int*)&parent)[0],((int*)&parent)[1]);
    fprintf(f,"c%08x%08x [",((int*)&p)[0],((int*)&p)[1]);
    if (fun)
      fun(f,p,arg);
    fprintf(f,"label=\"nil\"];\n");
    return;
  }

  c = get_pntr(p);

  if (pntrset_contains(done,p))
    return;
  pntrset_add(done,p);

  if (landscape && (CELL_APPLICATION == pntrtype(p))) {
    pntr app = resolve_pntr(p);
    pntr par = parent;
    fprintf(f,"subgraph {\n");
    fprintf(f,"rank=same;\n");

    while (CELL_APPLICATION == pntrtype(app)) {
      cell *appc = get_pntr(app);
      fprintf(f,"c%08x%08x [",((int*)&app)[0],((int*)&app)[1]);
      if (fun)
        fun(f,app,arg);
      fprintf(f,"label=\"@\"];\n");
      par = app;
      app = resolve_pntr(appc->field1);
    }
    dot_graph_r(f,app,done,doind,fun,arg,par,landscape);
    fprintf(f,"}\n");

    app = resolve_pntr(p);

    while (CELL_APPLICATION == pntrtype(app)) {
      cell *appc = get_pntr(app);
      dot_edge(f,resolve_pntr(appc->field1),app,doind,"dir=back");
      dot_edge(f,app,resolve_pntr(appc->field2),doind,NULL);
      dot_graph_r(f,resolve_pntr(appc->field2),done,doind,fun,arg,app,landscape);
      app = resolve_pntr(appc->field1);
    }
    return;
  }

  fprintf(f,"c%08x%08x [",((int*)&p)[0],((int*)&p)[1]);

  if (fun)
    fun(f,p,arg);

  fprintf(f,"label=\"");

  switch (pntrtype(p)) {
  case CELL_APPLICATION:
    fprintf(f,"@\"];\n");
    dot_graph_r(f,c->field1,done,doind,fun,arg,p,landscape);
    dot_graph_r(f,c->field2,done,doind,fun,arg,p,landscape);
    dot_edge(f,p,c->field1,doind,NULL);
    dot_edge(f,p,c->field2,doind,NULL);
    break;
  case CELL_CONS:
    fprintf(f,"CONS\"];\n");
    dot_graph_r(f,c->field1,done,doind,fun,arg,p,landscape);
    dot_graph_r(f,c->field2,done,doind,fun,arg,p,landscape);
    dot_edge(f,p,c->field1,doind,NULL);
    dot_edge(f,p,c->field2,doind,NULL);
    break;
  case CELL_IND:
    fprintf(f,"IND\"];\n");
    dot_graph_r(f,c->field1,done,doind,fun,arg,p,landscape);
    dot_edge(f,p,c->field1,doind,NULL);
    break;
  case CELL_BUILTIN:
    fprintf(f,"%s\"];\n",builtin_info[(int)get_pntr(c->field1)].name);
    break;
  case CELL_SCREF:
    fprintf(f,"%s\"];\n",((scomb*)get_pntr(c->field1))->name);
    break;
  case CELL_SYMBOL:
    fprintf(f,"%s\"];\n",(char*)get_pntr(c->field1));
    break;
  case CELL_AREF: {
    carray *arr = aref_array(p);
    if (1 == arr->elemsize) {
      pntr tail = resolve_pntr(arr->tail);
      char *val  = (char*)malloc(arr->size+1);
      memcpy(val,(char*)arr->elements,arr->size);
      val[arr->size] = '\0';
      fprintf(f,"\\\"");
      print_double_escaped(f,val);
      fprintf(f,"\\\"\"];\n");
      free(val);

      if (CELL_NIL != pntrtype(tail)) {
        dot_graph_r(f,arr->tail,done,doind,fun,arg,p,landscape);
        dot_edge(f,p,arr->tail,doind,"color=red");
      }
    }
    else {
      int i;
      fprintf(f,"[ARRAY]\"];\n");
      for (i = 0; i < arr->size; i++) {
        pntr elem = ((pntr*)arr->elements)[i];
        dot_graph_r(f,elem,done,doind,fun,arg,p,landscape);
        dot_edge(f,p,elem,doind,"color=red");
      }
      dot_graph_r(f,arr->tail,done,doind,fun,arg,p,landscape);
      dot_edge(f,p,arr->tail,doind,"color=red");
    }
    break;
  }
  case CELL_FRAME:
  case CELL_CAP:
    abort(); /* don't need to handle this case */
    break;
  case CELL_SYSOBJECT:
    fprintf(f,"sysobject(%s)\"];\n",sysobject_types[((sysobject*)get_pntr(c->field1))->type]);
    break;
  default:
    fprintf(f,"%s\"];\n",cell_types[pntrtype(p)]);
    break;
  }
}

void dot_graph(const char *prefix, int number, pntr root, int doind,
               dotfun fun, pntr arg, const char *msg, int landscape)
{
  FILE *f;
  char *filename = (char*)malloc(strlen(prefix)+50);
  pntrset *done = pntrset_new();

  sprintf(filename,"%s%04d.dot",prefix,number);

  if (NULL == (f = fopen(filename,"w"))) {
    perror(filename);
    exit(1);
  }

  if (landscape)
    fprintf(f,"//landscape\n");
  fprintf(f,"digraph {\n");
/*   if (landscape) */
/*     fprintf(f,"rankdir=LR;\n"); */
  fprintf(f,"fontsize=24;\n");
  fprintf(f,"color=white;\n");
  fprintf(f,"subgraph clustertop {\n");

  if (NULL != msg) {
    fprintf(f,"label=\"Step %d: ",number);
    print_escaped(f,msg);
    fprintf(f,"\";\n");
  }

  fprintf(f,"node [fontsize=24,color=white];\n");
  dot_graph_r(f,root,done,doind,fun,arg,NULL_PNTR,landscape);
  fprintf(f,"}\n");
  fprintf(f,"}\n");

  fclose(f);
  pntrset_free(done);
}

typedef struct tharg {
  pntr target;
  int allapps;
} tharg;

static void trace_hilight(FILE *f, pntr p, pntr arg1)
{
  tharg *arg = (tharg*)get_pntr(arg1);
  p = resolve_pntr(p);
  if (arg->allapps) {
    pntr app = arg->target;
    while (CELL_APPLICATION == pntrtype(app)) {
      pntr argval = resolve_pntr(get_pntr(app)->field2);
      if (pntrequal(p,arg->target))
        fprintf(f,"style=filled,fillcolor=blue,fontcolor=white,");
      else if (pntrequal(p,app))
        fprintf(f,"style=filled,fillcolor=blue,fontcolor=white,");
      else if (pntrequal(p,argval))
        fprintf(f,"style=dashed,color=black,");
      app = resolve_pntr(get_pntr(app)->field1);
    }
    if (pntrequal(p,app))
      fprintf(f,"style=filled,fillcolor=black,fontcolor=white,");
  }
  else {
    if (pntrequal(p,arg->target))
      fprintf(f,"style=filled,fillcolor=blue,fontcolor=white,");
  }
}

char *fmtstring(const char *format, va_list ap)
{
  va_list tmp;
  int len;
  char *str;
  va_copy(tmp,ap);
  len = vsnprintf(NULL,0,format,tmp);
  va_end(tmp);
  str = (char*)malloc(len+1);
  va_copy(tmp,ap);
  vsprintf(str,format,tmp);
  va_end(tmp);
  return str;
}

void trace_step(task *tsk, pntr target, int allapps, const char *format, ...)
{
  if (tsk->tracing) {
    tharg arg;
    pntr rhp;
    int landscape = (TRACE_LANDSCAPE == tsk->trace_type);
    char *prefix = (char*)malloc(strlen(tsk->trace_dir)+1+strlen(TRACE_PREFIX)+1);
    va_list ap;
    char *str;
    sprintf(prefix,"%s/%s",tsk->trace_dir,TRACE_PREFIX);

    va_start(ap,format);
    str = fmtstring(format,ap);
    va_end(ap);

    tsk->trace_steps++;

    arg.target = resolve_pntr(target);
    arg.allapps = allapps;
    make_pntr(rhp,&arg);
    printf("-------------------------------------------------\n");
    printf("Step %d: %s\n\n",tsk->trace_steps,str);
    print_graph(tsk->trace_src,tsk->trace_root);
    printf("\n");

    dot_graph(prefix,tsk->trace_steps,tsk->trace_root,0,trace_hilight,rhp,str,landscape);
    free(prefix);
    free(str);
  }
}
