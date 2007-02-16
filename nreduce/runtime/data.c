/*
 * This file is part of the GridXSLT project
 * Copyright (C) 2006 Peter Kelly (pmk@cs.adelaide.edu.au)
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
#include "compiler/bytecode.h"
#include "compiler/source.h"
#include "runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>

reader read_start(const char *data, int size)
{
  reader rd;
  rd.data = data;
  rd.size = size;
  rd.pos = 0;
  return rd;
}

static int read_bytes(reader *rd, void *data, int count)
{
  if (rd->pos+count > rd->size)
    return READER_INCOMPLETE_DATA;
  memcpy(data,&rd->data[rd->pos],count);
  rd->pos += count;
  return READER_OK;
}

static int read_check_tag(reader *rd, int tag)
{
  int got;
  CHECK_READ(read_bytes(rd,&got,sizeof(int)));
  if (got != tag)
    return READER_INCORRECT_TYPE;
  return READER_OK;
}

static int read_tagged_bytes(reader *rd, int tag, void *data, int count)
{
  CHECK_READ(read_check_tag(rd,tag));
  return read_bytes(rd,data,count);
}

int read_char(reader *rd, char *c)
{
  return read_tagged_bytes(rd,CHAR_TAG,c,sizeof(char));
}

int read_int(reader *rd, int *i)
{
  return read_tagged_bytes(rd,INT_TAG,i,sizeof(int));
}

int read_double(reader *rd, double *d)
{
  return read_tagged_bytes(rd,DOUBLE_TAG,d,sizeof(double));
}

int read_string(reader *rd, char **s)
{
  int len;
  CHECK_READ(read_check_tag(rd,STRING_TAG));
  CHECK_READ(read_bytes(rd,&len,sizeof(int)));
  if (rd->pos+len > rd->size)
    return READER_INVALID_DATA;
  *s = (char*)malloc(len+1);
  memcpy(*s,&rd->data[rd->pos],len);
  (*s)[len] = '\0';
  rd->pos += len;
  return READER_OK;
}

int read_binary(reader *rd, void **b, int *len)
{
  CHECK_READ(read_check_tag(rd,BINARY_TAG));
  CHECK_READ(read_bytes(rd,len,sizeof(int)));
  if (rd->pos+*len > rd->size)
    return READER_INVALID_DATA;
  *b = malloc(*len);
  memcpy(*b,&rd->data[rd->pos],*len);
  rd->pos += *len;
  return READER_OK;
}

int read_gaddr_noack(reader *rd, gaddr *a)
{
  CHECK_READ(read_check_tag(rd,GADDR_TAG));
  CHECK_READ(read_bytes(rd,&a->pid,sizeof(int)));
  CHECK_READ(read_bytes(rd,&a->lid,sizeof(int)));
  return READER_OK;
}

int read_gaddr(reader *rd, task *tsk, gaddr *a)
{
  assert(tsk->ackmsg);
  CHECK_READ(read_gaddr_noack(rd,a));
  if ((-1 != a->pid) || (-1 != a->lid)) {
    write_gaddr_noack(tsk->ackmsg,*a);
    tsk->naddrsread++;
  }
  return READER_OK;
}

int read_pntr(reader *rd, task *tsk, pntr *pout, int observe)
{
  /* TODO: determine if this refers to an object we already have a copy of, and return
     that object instead. (see Trinder96 2.3.2) */
  gaddr addr;
  int type;
  CHECK_READ(read_check_tag(rd,PNTR_TAG));
  CHECK_READ(read_format(rd,tsk,observe,"ai",&addr,&type));

  switch (type) {
  case CELL_IND:
    /* shouldn't receive an IND cell */
    return READER_INCORRECT_CONTENTS;
  case CELL_AREF:
    /* shouldn't receive an AREF cell (yet...) */
    return READER_INCORRECT_CONTENTS;
  case CELL_CONS: {
    cell *c;
    pntr head;
    pntr tail;

    CHECK_READ(read_format(rd,tsk,observe,"pp",&head,&tail));

    c = alloc_cell(tsk);
    c->type = CELL_CONS;
    c->field1 = head;
    c->field2 = tail;
    make_pntr(*pout,c);
    break;
  }
  case CELL_REMOTEREF:
    make_pntr(*pout,NULL);
    *pout = global_lookup(tsk,addr,*pout);
    break;
  case CELL_HOLE:
    fatal("shouldn't receive HOLE");
    break;
  case CELL_FRAME: {
    frame *fr = frame_new(tsk);
    int i;
    int count;
    int address;

    fr->c = alloc_cell(tsk);
    fr->c->type = CELL_FRAME;
    make_pntr(fr->c->field1,fr);
    make_pntr(*pout,fr->c);

    CHECK_READ(read_format(rd,tsk,observe,"iii",&address,&fr->fno,&fr->alloc));
    fr->instr = bc_instructions(tsk->bcdata)+address;

    count = fr->instr->expcount;
    if ((count > fr->alloc) || (MAX_FRAME_SIZE <= fr->alloc))
      return READER_INCORRECT_CONTENTS;

    assert(fr->alloc == tsk->maxstack);
    for (i = 0; i < count; i++)
      CHECK_READ(read_pntr(rd,tsk,&fr->data[i],observe));
    break;
  }
  case CELL_CAP: {
    cap *cp = cap_alloc(1,0,0);
    cell *capcell;
    int i;

    capcell = alloc_cell(tsk);
    capcell->type = CELL_CAP;
    make_pntr(capcell->field1,cp);
    make_pntr(*pout,capcell);

    CHECK_READ(read_format(rd,tsk,observe,"iiiiii",&cp->arity,&cp->address,&cp->fno,
                           &cp->sl.fileno,&cp->sl.lineno,&cp->count));

    if (MAX_CAP_SIZE <= cp->count)
      return READER_INCORRECT_CONTENTS;

    cp->data = (pntr*)calloc(cp->count,sizeof(pntr));
    for (i = 0; i < cp->count; i++)
      CHECK_READ(read_pntr(rd,tsk,&cp->data[i],observe));
    break;
  }
  case CELL_NIL:
    *pout = tsk->globnilpntr;
    break;
  case CELL_NUMBER:
    CHECK_READ(read_double(rd,pout));
    assert(!is_pntr(*pout));
    break;
  case CELL_SYSOBJECT:
    /* FIXME: these should't migrate... how to handle them? */
    abort();
    break;
  default:
    return READER_INCORRECT_CONTENTS;
  }

  return READER_OK;
}

int read_vformat(reader *rd, task *tsk, int observe, const char *fmt, va_list ap)
{
  int r = READER_OK;
  void **allocs = (void**)calloc(strlen(fmt),sizeof(void*));
  int done = 0;

  assert(tsk->ackmsg);

  for (; *fmt && (READER_OK == r); fmt++) {
    switch (*fmt) {
    case 'c':
      r = read_char(rd,va_arg(ap,char*));
      break;
    case 'i':
      r = read_int(rd,va_arg(ap,int*));
      break;
    case 'd':
      r = read_double(rd,va_arg(ap,double*));
      break;
    case 's': {
      char **sptr = va_arg(ap,char**);
      r = read_string(rd,sptr);
      if (READER_OK == r)
        allocs[done] = *sptr;
      break;
    }
    case 'b': {
      void **bptr = va_arg(ap,void**);
      int *lenptr = va_arg(ap,int*);
      r = read_binary(rd,bptr,lenptr);
      if (READER_OK == r)
        allocs[done] = *bptr;
      break;
    }
    case 'a':
      r = read_gaddr(rd,tsk,va_arg(ap,gaddr*));
      break;
    case 'p':
      r = read_pntr(rd,tsk,(va_arg(ap,pntr*)),observe);
      break;
    case 'r': {
      pntr *p = (va_arg(ap,pntr*));
      r = read_pntr(rd,tsk,p,observe);
      if ((READER_OK == r) && (CELL_REMOTEREF != pntrtype(*p)))
        r = READER_INCORRECT_CONTENTS;
      break;
    }
    case '.':
      r = read_end(rd);
      break;
    default:
      fatal("invalid read format character");
    }
    if (READER_OK == r)
      done++;
  }

  if (READER_OK != r) {
    printf("INVALID DATA -- FREEING STRINGS\n");
    for (done--; 0 <= done; done--)
      free(allocs[done]);
  }
  free(allocs);
  return r;
}

int read_format(reader *rd, task *tsk, int observe, const char *fmt, ...)
{
  int r;
  va_list ap;
  va_start(ap,fmt);
  r = read_vformat(rd,tsk,observe,fmt,ap);
  va_end(ap);
  return r;
}

int print_data(task *tsk, const char *data, int size)
{
  FILE *f = tsk->output;
  reader rd = read_start(data,size);
  int r;
  int count = 0;

  assert(NULL == tsk->ackmsg);
  assert(0 == tsk->ackmsgsize);
  assert(0 == tsk->naddrsread);
  tsk->ackmsg = write_start();

  while (rd.pos < rd.size) {
    int tag;
    if (0 < count++)
      fprintf(f," ");
    if (READER_OK != (r = read_bytes(&rd,&tag,sizeof(int))))
      return r;
    rd.pos -= sizeof(int);
    switch (tag) {
    case CHAR_TAG: {
      char c;
      CHECK_READ(read_char(&rd,&c));
      fprintf(f,"%d",c);
      break;
    }
    case INT_TAG: {
      int i;
      CHECK_READ(read_int(&rd,&i));
      fprintf(f,"%d",i);
      break;
    }
    case DOUBLE_TAG: {
      double d;
      CHECK_READ(read_double(&rd,&d));
      fprintf(f,"%f",d);
      break;
    }
    case STRING_TAG: {
      char *str;
      CHECK_READ(read_string(&rd,&str));
      fprintf(f,"\"%s\"",str);
      free(str);
      break;
    }
    case GADDR_TAG: {
      gaddr a;
      CHECK_READ(read_gaddr(&rd,tsk,&a));
      fprintf(f,"%d@%d",a.lid,a.pid);
      break;
    }
    case PNTR_TAG: {
      pntr p;
      CHECK_READ(read_format(&rd,tsk,1,"p",&p));
      fprintf(f,"#");
      print_pntr(f,p);
      break;
    }
    default:
      fprintf(f,"Unknown type: %d\n",tag);
      return READER_INCORRECT_TYPE;
    }
  }

  write_end(tsk->ackmsg); /* don't want to send acks in this case */
  tsk->ackmsg = NULL;
  tsk->naddrsread = 0;
  tsk->ackmsgsize = 0;

  return READER_OK;
}

int read_end(reader *rd)
{
  if (rd->size > rd->pos)
    return READER_EXTRA_DATA;
  return 0;
}

array *write_start(void)
{
  return array_new(sizeof(char));
}

void write_tag(array *wr, int tag)
{
  array_append(wr,&tag,sizeof(int));
}

void write_char(array *wr, char c)
{
  write_tag(wr,CHAR_TAG);
  array_append(wr,&c,sizeof(char));
}

void write_int(array *wr, int i)
{
  write_tag(wr,INT_TAG);
  array_append(wr,&i,sizeof(int));
}

void write_double(array *wr, double d)
{
  write_tag(wr,DOUBLE_TAG);
  array_append(wr,&d,sizeof(d));
}

void write_string(array *wr, char *s)
{
  int len = strlen(s);
  write_tag(wr,STRING_TAG);
  array_append(wr,&len,sizeof(int));
  array_append(wr,s,len);
}

void write_binary(array *wr, const void *b, int len)
{
  write_tag(wr,BINARY_TAG);
  array_append(wr,&len,sizeof(int));
  array_append(wr,b,len);
}

void write_gaddr_noack(array *wr, gaddr a)
{
  write_tag(wr,GADDR_TAG);
  array_append(wr,&a.pid,sizeof(int));
  array_append(wr,&a.lid,sizeof(int));
}

void write_gaddr(array *wr, task *tsk, gaddr a)
{
  add_gaddr(&tsk->inflight,a);
  write_gaddr_noack(wr,a);
}

void write_ref(array *arr, task *tsk, pntr p)
{
  write_tag(arr,PNTR_TAG);
  if (is_pntr(p)) {
    global *glo = make_global(tsk,p);
    write_format(arr,tsk,"ai",glo->addr,CELL_REMOTEREF);
  }
  else {
    gaddr addr;
    addr.pid = -1;
    addr.lid = -1;
    write_format(arr,tsk,"aid",addr,CELL_NUMBER,p);
  }
}

void write_pntr(array *arr, task *tsk, pntr p)
{
  gaddr addr;
  addr.pid = -1;
  addr.lid = -1;

  if (CELL_IND == pntrtype(p)) {
    write_pntr(arr,tsk,get_pntr(p)->field1);
    return;
  }

  if (CELL_REMOTEREF == pntrtype(p)) {
    global *glo = (global*)get_pntr(get_pntr(p)->field1);
    if (0 <= glo->addr.lid) {
      write_tag(arr,PNTR_TAG);
      write_format(arr,tsk,"ai",glo->addr,CELL_REMOTEREF);
    }
    else {
      /* FIXME: how to deal with this case? */
      /* Want to make another ref which points to this */
      abort();
      write_ref(arr,tsk,p);
    }
    return;
  }

  if (CELL_NUMBER != pntrtype(p))
    addr = global_addressof(tsk,p);


  write_tag(arr,PNTR_TAG);

  if (CELL_AREF == pntrtype(p)) {
    cell *c = get_pntr(p);
    cell *arrholder = get_pntr(c->field1);
    carray *carr = aref_array(p);
    int index = aref_index(p);

    assert(sizeof(pntr) == carr->elemsize); /* FIXME: remove this restriction */

    if (index+1 < carr->size) {
      pntr p;
      make_aref_pntr(p,arrholder,index+1);
      write_format(arr,tsk,"airr",addr,CELL_CONS,
                   ((pntr*)carr->elements)[index],p);
    }
    else {
      write_format(arr,tsk,"airr",addr,CELL_CONS,
                   ((pntr*)carr->elements)[index],carr->tail);
    }
    return;
  }

  write_format(arr,tsk,"ai",addr,pntrtype(p));
  switch (pntrtype(p)) {
  case CELL_CONS: {
    write_ref(arr,tsk,get_pntr(p)->field1);
    write_ref(arr,tsk,get_pntr(p)->field2);
    break;
  }
  case CELL_HOLE:
    write_ref(arr,tsk,p);
    break;
  case CELL_FRAME: {
    frame *f = (frame*)get_pntr(get_pntr(p)->field1);
    int i;
    int count = f->instr->expcount;
    int address = f->instr-bc_instructions(tsk->bcdata);

    /* FIXME: if frame is sparked, schedule it; if frame is running, just send a ref */
    assert(STATE_NEW == f->state);
    assert(!f->prev && !f->next);

    write_format(arr,tsk,"iii",address,f->fno,f->alloc);
    for (i = 0; i < count; i++) {
      pntr arg = resolve_pntr(f->data[i]);
      if ((CELL_NUMBER == pntrtype(arg)) || (CELL_NIL == pntrtype(arg)))
        write_pntr(arr,tsk,f->data[i]);
      else
        write_ref(arr,tsk,f->data[i]);
    }
    break;
  }
  case CELL_CAP: {
    cap *cp = (cap*)get_pntr(get_pntr(p)->field1);
    int i;
    write_format(arr,tsk,"iiiiii",cp->arity,cp->address,cp->fno,
                 cp->sl.fileno,cp->sl.lineno,cp->count);
    for (i = 0; i < cp->count; i++)
      write_ref(arr,tsk,cp->data[i]);
    break;
  }
  case CELL_NIL:
    break;
  case CELL_NUMBER:
    write_double(arr,p);
    break;
  case CELL_SYSOBJECT:
    /* FIXME: these should't migrate... how to handle them? */
    abort();
    break;
  default:
    abort();
    break;
  }
}

void write_vformat(array *wr, task *tsk, const char *fmt, va_list ap)
{
  for (; *fmt; fmt++) {
    switch (*fmt) {
    case 'c':
      write_char(wr,(char)(va_arg(ap,int)));
      break;
    case 'i':
      write_int(wr,va_arg(ap,int));
      break;
    case 'd':
      write_double(wr,va_arg(ap,double));
      break;
    case 's':
      write_string(wr,va_arg(ap,char*));
      break;
    case 'b': {
      const void *b = va_arg(ap,const void*);
      int len = va_arg(ap,int);
      write_binary(wr,b,len);
      break;
    }
    case 'a':
      write_gaddr(wr,tsk,va_arg(ap,gaddr));
      break;
    case 'p':
      write_pntr(wr,tsk,va_arg(ap,pntr));
      break;
    case 'r':
      write_ref(wr,tsk,va_arg(ap,pntr));
      break;
    default:
      fatal("invalid write format character");
    }
  }
}

void write_format(array *wr, task *tsk, const char *fmt, ...)
{
  va_list ap;
  va_start(ap,fmt);
  write_vformat(wr,tsk,fmt,ap);
  va_end(ap);
}

void write_end(array *wr)
{
  array_free(wr);
}

void msg_print(task *tsk, int dest, int tag, const char *data, int size)
{
  reader rd = read_start(data,size);
  int r;

  fprintf(tsk->output,"%d <= %s ",dest,msg_names[tag]);
  r = print_data(tsk,data+rd.pos,size-rd.pos);
  assert(READER_OK == r);
  fprintf(tsk->output,"\n");
}

void msg_send(task *tsk, int dest, int tag, char *data, int size)
{
  if (NULL != tsk->inflight) {
    int count = 0;
    list *l;
    for (l = tsk->inflight; l; l = l->next) {
      gaddr *addr = (gaddr*)l->data;
      array_append(tsk->inflight_addrs[dest],addr,sizeof(gaddr));
      count++;
    }
    array_append(tsk->unack_msg_acount[dest],&count,sizeof(int));
    list_free(tsk->inflight,free);
    tsk->inflight = NULL;
  }

  socket_send(tsk,dest,tag,data,size);
}

void msg_fsend(task *tsk, int dest, int tag, const char *fmt, ...)
{
  va_list ap;
  array *wr;

  wr = write_start();
  va_start(ap,fmt);
  write_vformat(wr,tsk,fmt,ap);
  va_end(ap);

  msg_send(tsk,dest,tag,wr->data,wr->nbytes);
  write_end(wr);
}

int msg_recv(task *tsk, int *tag, char **data, int *size, int delayms)
{
  return socket_recv(tsk,tag,data,size,delayms);
}
