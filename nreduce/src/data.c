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
 * $Id: memory.c 330 2006-08-23 06:15:19Z pmkelly $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define MEMORY_C

#include "grammar.tab.h"
#include "nreduce.h"
#include "gcode.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <mpi.h>

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

int read_gaddr_noack(reader *rd, gaddr *a)
{
  CHECK_READ(read_check_tag(rd,GADDR_TAG));
  CHECK_READ(read_bytes(rd,&a->pid,sizeof(int)));
  CHECK_READ(read_bytes(rd,&a->lid,sizeof(int)));
  return READER_OK;
}

int read_gaddr(reader *rd, process *proc, gaddr *a)
{
  assert(proc->ackmsg);
  CHECK_READ(read_gaddr_noack(rd,a));
  if ((-1 != a->pid) || (-1 != a->lid)) {
    write_gaddr_noack(proc->ackmsg,*a);
    proc->naddrsread++;
  }
  return READER_OK;
}

int read_pntr(reader *rd, process *proc, pntr *pout, int observe)
{
  /* TODO: determine if this refers to an object we already have a copy of, and return
     that object instead. (see Trinder96 2.3.2) */
  gaddr addr;
  int type;
  CHECK_READ(read_check_tag(rd,PNTR_TAG));
  CHECK_READ(read_format(rd,proc,observe,"ai",&addr,&type));

  switch (type) {
  case TYPE_IND:
    /* shouldn't receive an IND cell */
    return READER_INCORRECT_CONTENTS;
  case TYPE_AREF:
    /* shouldn't receive an AREF cell (yet...) */
    return READER_INCORRECT_CONTENTS;
  case TYPE_CONS: {
    cell *c;
    pntr head;
    pntr tail;

    CHECK_READ(read_format(rd,proc,observe,"pp",&head,&tail));

    c = alloc_cell(proc);
    c->type = TYPE_CONS;
    c->field1 = head;
    c->field2 = tail;
    make_pntr(*pout,c);
    break;
  }
  case TYPE_REMOTEREF:
    make_pntr(*pout,NULL);
    *pout = global_lookup(proc,addr,*pout);
    break;
  case TYPE_HOLE:
    fatal("shouldn't receive HOLE");
    break;
  case TYPE_FRAME: {
    frame *fr = frame_alloc(proc);
    int i;

    fr->c = alloc_cell(proc);
    fr->c->type = TYPE_FRAME;
    make_pntr(fr->c->field1,fr);
    make_pntr(*pout,fr->c);

    CHECK_READ(read_format(rd,proc,observe,"iiii",&fr->address,&fr->fno,&fr->alloc,&fr->count));

    if ((fr->count > fr->alloc) || (MAX_FRAME_SIZE <= fr->alloc))
      return READER_INCORRECT_CONTENTS;

    fr->data = (pntr*)calloc(fr->alloc,sizeof(pntr));
    for (i = 0; i < fr->count; i++)
      CHECK_READ(read_pntr(rd,proc,&fr->data[i],observe));
    break;
  }
  case TYPE_CAP: {
    cap *cp = cap_alloc(1,0,0);
    cell *capcell;
    int i;

    capcell = alloc_cell(proc);
    capcell->type = TYPE_CAP;
    make_pntr(capcell->field1,cp);
    make_pntr(*pout,capcell);

    CHECK_READ(read_format(rd,proc,observe,"iiiiii",&cp->arity,&cp->address,&cp->fno,
                           &cp->sl.fileno,&cp->sl.lineno,&cp->count));

    if (MAX_CAP_SIZE <= cp->count)
      return READER_INCORRECT_CONTENTS;

    cp->data = (pntr*)calloc(cp->count,sizeof(pntr));
    for (i = 0; i < cp->count; i++)
      CHECK_READ(read_pntr(rd,proc,&cp->data[i],observe));
    break;
  }
  case TYPE_NIL:
    *pout = proc->globnilpntr;
    break;
  case TYPE_NUMBER:
    CHECK_READ(read_double(rd,pout));
    assert(!is_pntr(*pout));
    break;
  case TYPE_STRING: {
    cell *c;
    char *str;
    CHECK_READ(read_string(rd,&str));

    c = alloc_cell(proc);
    c->type = TYPE_STRING;
    make_string(c->field1,str);
    make_pntr(*pout,c);
    break;
  }
  default:
    return READER_INCORRECT_CONTENTS;
  }

  return READER_OK;
}

int read_vformat(reader *rd, process *proc, int observe, const char *fmt, va_list ap)
{
  int r = READER_OK;
  void **allocs = (void**)calloc(strlen(fmt),sizeof(void*));
  int done = 0;

  assert(proc->ackmsg);

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
    case 'a':
      r = read_gaddr(rd,proc,va_arg(ap,gaddr*));
      break;
    case 'p':
      r = read_pntr(rd,proc,(va_arg(ap,pntr*)),observe);
      break;
    case 'r': {
      pntr *p = (va_arg(ap,pntr*));
      r = read_pntr(rd,proc,p,observe);
      if ((READER_OK == r) && (TYPE_REMOTEREF != pntrtype(*p)))
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

int read_format(reader *rd, process *proc, int observe, const char *fmt, ...)
{
  int r;
  va_list ap;
  va_start(ap,fmt);
  r = read_vformat(rd,proc,observe,fmt,ap);
  va_end(ap);
  return r;
}

int print_data(process *proc, const char *data, int size)
{
  FILE *f = proc->output;
  reader rd = read_start(data,size);
  int r;
  int count = 0;

  assert(NULL == proc->ackmsg);
  assert(0 == proc->ackmsgsize);
  assert(0 == proc->naddrsread);
  proc->ackmsg = write_start();

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
      CHECK_READ(read_gaddr(&rd,proc,&a));
      fprintf(f,"%d@%d",a.lid,a.pid);
      break;
    }
    case PNTR_TAG: {
      pntr p;
      CHECK_READ(read_format(&rd,proc,1,"p",&p));
      fprintf(f,"#");
      print_pntr(f,p);
      break;
    }
    default:
      fprintf(f,"Unknown type: %d\n",tag);
      return READER_INCORRECT_TYPE;
    }
  }

  write_end(proc->ackmsg); /* don't want to send acks in this case */
  proc->ackmsg = NULL;
  proc->naddrsread = 0;
  proc->ackmsgsize = 0;

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

void write_gaddr_noack(array *wr, gaddr a)
{
  write_tag(wr,GADDR_TAG);
  array_append(wr,&a.pid,sizeof(int));
  array_append(wr,&a.lid,sizeof(int));
}

void write_gaddr(array *wr, process *proc, gaddr a)
{
  add_gaddr(&proc->inflight,a);
  write_gaddr_noack(wr,a);
}

void write_ref(array *arr, process *proc, pntr p)
{
  write_tag(arr,PNTR_TAG);
  if (is_pntr(p)) {
    global *glo = make_global(proc,p);
    write_format(arr,proc,"ai",glo->addr,TYPE_REMOTEREF);
  }
  else {
    gaddr addr;
    addr.pid = -1;
    addr.lid = -1;
    write_format(arr,proc,"aid",addr,TYPE_NUMBER,p);
  }
}

void write_pntr(array *arr, process *proc, pntr p)
{
  gaddr addr;
  addr.pid = -1;
  addr.lid = -1;

  if (TYPE_IND == pntrtype(p)) {
    write_pntr(arr,proc,get_pntr(p)->field1);
    return;
  }

  if (TYPE_REMOTEREF == pntrtype(p)) {
    global *glo = (global*)get_pntr(get_pntr(p)->field1);
    if (0 <= glo->addr.lid) {
      write_tag(arr,PNTR_TAG);
      write_format(arr,proc,"ai",glo->addr,TYPE_REMOTEREF);
    }
    else {
      /* FIXME: how to deal with this case? */
      /* Want to make another ref which points to this */
      abort();
      write_ref(arr,proc,p);
    }
    return;
  }

  if (TYPE_NUMBER != pntrtype(p))
    addr = global_addressof(proc,p);


  write_tag(arr,PNTR_TAG);

  if (TYPE_AREF == pntrtype(p)) {
    cell *c = get_pntr(p);
    cell *arrholder = get_pntr(c->field1);
    carray *carr = (carray*)get_pntr(arrholder->field1);
    int index = (int)get_pntr(c->field2);
    if (index+1 < carr->size)
      write_format(arr,proc,"airr",addr,TYPE_CONS,
                   carr->elements[index],get_aref(proc,arrholder,index+1));
    else
      write_format(arr,proc,"airr",addr,TYPE_CONS,
                   carr->elements[index],carr->tail);
    return;
  }

  write_format(arr,proc,"ai",addr,pntrtype(p));
  switch (pntrtype(p)) {
  case TYPE_CONS: {
    write_ref(arr,proc,get_pntr(p)->field1);
    write_ref(arr,proc,get_pntr(p)->field2);
    break;
  }
  case TYPE_HOLE:
    write_ref(arr,proc,p);
    break;
  case TYPE_FRAME: {
    frame *f = (frame*)get_pntr(get_pntr(p)->field1);
    int i;

    /* FIXME: if frame is sparked, schedule it; if frame is running, just send a ref */
    assert(STATE_NEW == f->state);
    assert(!f->qprev && !f->qnext);

    write_format(arr,proc,"iiii",f->address,f->fno,f->alloc,f->count);
    for (i = 0; i < f->count; i++) {
      pntr arg = resolve_pntr(f->data[i]);
      if ((TYPE_NUMBER == pntrtype(arg)) || (TYPE_NIL == pntrtype(arg)))
        write_pntr(arr,proc,f->data[i]);
      else
        write_ref(arr,proc,f->data[i]);
    }
    break;
  }
  case TYPE_CAP: {
    cap *cp = (cap*)get_pntr(get_pntr(p)->field1);
    int i;
    write_format(arr,proc,"iiiiii",cp->arity,cp->address,cp->fno,
                 cp->sl.fileno,cp->sl.lineno,cp->count);
    for (i = 0; i < cp->count; i++)
      write_ref(arr,proc,cp->data[i]);
    break;
  }
  case TYPE_NIL:
    break;
  case TYPE_NUMBER:
    write_double(arr,p);
    break;
  case TYPE_STRING:
    write_string(arr,get_string(get_pntr(p)->field1));
    break;
  default:
    abort();
    break;
  }
}

void write_vformat(array *wr, process *proc, const char *fmt, va_list ap)
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
    case 'a':
      write_gaddr(wr,proc,va_arg(ap,gaddr));
      break;
    case 'p':
      write_pntr(wr,proc,va_arg(ap,pntr));
      break;
    case 'r':
      write_ref(wr,proc,va_arg(ap,pntr));
      break;
    default:
      fatal("invalid write format character");
    }
  }
}

void write_format(array *wr, process *proc, const char *fmt, ...)
{
  va_list ap;
  va_start(ap,fmt);
  write_vformat(wr,proc,fmt,ap);
  va_end(ap);
}

void write_end(array *wr)
{
  array_free(wr);
}

#ifdef MSG_DEBUG
static void msg_print(process *proc, int dest, int tag, const char *data, int size)
{
  reader rd = read_start(data,size);
  int r;

  fprintf(proc->output,"%d <= %s ",dest,msg_names[tag]);
  r = print_data(proc,data+rd.pos,size-rd.pos);
  assert(READER_OK == r);
  fprintf(proc->output,"\n");
}
#endif

#ifndef USE_MPI
static void msg_send_addcount(process *proc, int tag, const char *data, int size)
{
  assert(0 <= tag);
  assert(MSG_COUNT > tag);
  proc->stats.sendcount[tag]++;
  proc->stats.sendbytes[tag] += size;
}
#endif

void msg_send(process *proc, int dest, int tag, char *data, int size)
{
  #ifdef MSG_DEBUG
  msg_print(proc,dest,tag,data,size);
  #endif

  if (NULL != proc->inflight) {
    int count = 0;
    list *l;
    for (l = proc->inflight; l; l = l->next) {
      gaddr *addr = (gaddr*)l->data;
      array_append(proc->inflight_addrs[dest],addr,sizeof(gaddr));
      count++;
    }
    array_append(proc->unack_msg_acount[dest],&count,sizeof(int));
    list_free(proc->inflight,free);
    proc->inflight = NULL;
  }

#ifndef USE_MPI
  message *msg = (message*)calloc(1,sizeof(message));
  process *destproc;
  msg->from = proc->pid;
  msg->to = dest;
  msg->tag = tag;
  msg->size = size;
  msg->data = (char*)malloc(size);
  memcpy(msg->data,data,size);

  msg_send_addcount(proc,tag,data,size);

  assert(0 <= dest);
  assert(dest < proc->grp->nprocs);
  destproc = proc->grp->procs[dest];

/*   fprintf(proc->output,"msg_send %d -> %d : data %p, size %d\n", */
/*           proc->pid,dest,msg->data,msg->size); */

  pthread_mutex_lock(&destproc->msglock);
  list_append(&destproc->msgqueue,msg);
  pthread_cond_signal(&destproc->msgcond);
  pthread_mutex_unlock(&destproc->msglock);
#else
/*   fprintf(proc->output,"msg_send 1\n"); */
  MPI_Send(data,size,MPI_BYTE,dest,tag,MPI_COMM_WORLD);
/*   fprintf(proc->output,"msg_send 2\n"); */
#endif
}

void msg_fsend(process *proc, int dest, int tag, const char *fmt, ...)
{
  va_list ap;
  array *wr;

  wr = write_start();
  va_start(ap,fmt);
  write_vformat(wr,proc,fmt,ap);
  va_end(ap);

  msg_send(proc,dest,tag,wr->data,wr->nbytes);
  write_end(wr);
}

#ifndef USE_MPI
static int msg_recv2(process *proc, int *tag, char **data, int *size, int block,
                     struct timespec *abstime)
{
  list *l = NULL;
  message *msg;
  int from;

  pthread_mutex_lock(&proc->msglock);
  if (block && (NULL == proc->msgqueue)) {
    if (abstime) {
      pthread_cond_timedwait(&proc->msgcond,&proc->msglock,abstime);
    }
    else {
      pthread_cond_wait(&proc->msgcond,&proc->msglock);
      assert(proc->msgqueue);
    }
  }
  if (NULL != (l = proc->msgqueue))
    proc->msgqueue = l->next;
  pthread_mutex_unlock(&proc->msglock);

  if (NULL == l)
    return -1;

  msg = (message*)l->data;
/*   fprintf(proc->output,"msg_recv %d -> %d : data %p, size %d\n", */
/*           msg->from,proc->pid,msg->data,msg->size); */

  *tag = msg->tag;
  *data = msg->data;
  *size = msg->size;
  from = msg->from;
  free(msg);
  free(l);

  return from;
}
#endif

#ifdef USE_MPI
#define RECV_BUFSIZE 1048576
//#define RECV_BUFSIZE 1024
static char recvbuf[RECV_BUFSIZE];
static MPI_Request nbrequest = MPI_REQUEST_NULL;
#endif

int msg_recv(process *proc, int *tag, char **data, int *size)
{
#ifndef USE_MPI
  return msg_recv2(proc,tag,data,size,0,NULL);
#else
  int r;
  MPI_Status status;
  int count;
  int flag = 0;

/*   fprintf(proc->output,"msg_recv\n"); */

  if (MPI_REQUEST_NULL == nbrequest) {
    r = MPI_Irecv(recvbuf,RECV_BUFSIZE,MPI_BYTE,MPI_ANY_SOURCE,MPI_ANY_TAG,
                  MPI_COMM_WORLD,&nbrequest);
/*     fprintf(proc->output,"MPI_Irecv returned %d\n",r); */
    if (MPI_SUCCESS != r) {
/*       perror("MPI_Irecv"); */
/*       fprintf(proc->output,"msg_recv return 1\n"); */
      return -1;
/*       abort(); */
    }
  }

  r = MPI_Test(&nbrequest,&flag,&status);
/*   fprintf(proc->output,"MPI_Test returned %d\n",r); */
  if (MPI_SUCCESS != r) {
/*     perror("MPI_Test"); */
/*     fprintf(proc->output,"msg_recv return 2\n"); */
    return -1;
/*     abort(); */
  }

  if (!flag)
    return -1;
  nbrequest = MPI_REQUEST_NULL;

  MPI_Get_count(&status,MPI_BYTE,&count);
/*   fprintf(proc->output,"count = %d\n",count); */

  *tag = status.MPI_TAG;
  *size = count;
  *data = malloc(count);
  memcpy(*data,recvbuf,count);

/*   fprintf(proc->output,"msg_recv return 3\n"); */
  return status.MPI_SOURCE;
#endif
}

int msg_recvb(process *proc, int *tag, char **data, int *size)
{
#ifndef USE_MPI
  return msg_recv2(proc,tag,data,size,1,NULL);
#else
  int r;
  MPI_Status status;
  int count;
/*   int x; */
/*   fprintf(proc->output,"msg_recvb\n"); */

  r = MPI_Recv(recvbuf,RECV_BUFSIZE,MPI_BYTE,MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&status);
/*   r = MPI_Recv(&x,1,MPI_INT,MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&status); */
/*   fprintf(proc->output,"MPI_Recv returned %d\n",r); */
  if (MPI_SUCCESS != r) {
/*     fprintf(proc->output,"msg_recvb return 1\n"); */
    return -1;
  }

  MPI_Get_count(&status,MPI_BYTE,&count);
/*   fprintf(proc->output,"count = %d\n",count); */

  *tag = status.MPI_TAG;
  *size = count;
  *data = malloc(count);
  memcpy(*data,recvbuf,count);

/*   fprintf(proc->output,"msg_recvb return 2\n"); */
  return status.MPI_SOURCE;
#endif
}

int msg_recvbt(process *proc, int *tag, char **data, int *size, struct timespec *abstime)
{
#ifndef USE_MPI
  return msg_recv2(proc,tag,data,size,1,abstime);
#else
  int r;

  while (0 > (r = msg_recv(proc,tag,data,size))) {
    struct timeval now;
    struct timespec sleep;
    gettimeofday(&now,NULL);

    if ((now.tv_sec > abstime->tv_sec) ||
        ((now.tv_sec == abstime->tv_sec) &&
         (now.tv_usec*1000 > abstime->tv_nsec)))
      return -1;

    sleep.tv_sec = 0;
    sleep.tv_nsec = 10*1000*1000;
    nanosleep(&sleep,NULL);
    #ifdef MSG_DEBUG
/*     fprintf(proc->output,"sleeping\n"); */
    #endif
  }

  return r;
#endif
}
