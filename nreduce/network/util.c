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

#define _GNU_SOURCE /* for PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "src/nreduce.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <fenv.h>

void fatal(const char *format, ...)
{
  va_list ap;
  fprintf(stderr,"fatal error: ");
  va_start(ap,format);
  vfprintf(stderr,format,ap);
  va_end(ap);
  fprintf(stderr,"\n");
  abort();
}

array *array_new(int elemsize, int initroom)
{
  array *arr = (array*)malloc(sizeof(array));
  arr->elemsize = elemsize;
  if (0 < initroom)
    arr->alloc = elemsize*initroom;
  else
    arr->alloc = 1024;
  arr->nbytes = 0;
  arr->data = malloc(arr->alloc);
  return arr;
}

int array_equals(array *a, array *b)
{
  return ((a->elemsize == b->elemsize) &&
          (a->nbytes == b->nbytes) &&
          !memcmp(a->data,b->data,a->nbytes));
}

void array_mkroom(array *arr, const int size)
{
  if (arr->nbytes+size > arr->alloc) {
    while (arr->nbytes+size > arr->alloc)
      arr->alloc *= 2;
    arr->data = realloc(arr->data,arr->alloc);
  }
}

void array_append(array *arr, const void *data, int size)
{
  array_mkroom(arr,size);
  memmove((char*)(arr->data)+arr->nbytes,data,size);
  arr->nbytes += size;
}

void array_vprintf(array *arr, const char *format, va_list ap)
{
  assert(1 == arr->elemsize);
  while (1) {
    va_list tmp;
    int r;

    va_copy(tmp,ap);
    r = vsnprintf(&arr->data[arr->nbytes],arr->alloc-arr->nbytes,format,ap);
    va_end(tmp);

    if ((0 > r) || (r >= arr->alloc-arr->nbytes)) {
      arr->alloc *= 2;
      arr->data = realloc(arr->data,arr->alloc);
    }
    else {
      arr->nbytes += r;
      return;
    }
  }
}

void array_printf(array *arr, const char *format, ...)
{
  va_list ap;
  va_start(ap,format);
  array_vprintf(arr,format,ap);
  va_end(ap);
}

void array_remove_data(array *arr, int nbytes)
{
  assert(nbytes <= arr->nbytes);
  memmove(&arr->data[0],&arr->data[nbytes],arr->nbytes-nbytes);
  arr->nbytes -= nbytes;
}

void array_remove_items(array *arr, int count)
{
  array_remove_data(arr,count*arr->elemsize);
}

int array_count(array *arr)
{
  return arr->nbytes/arr->elemsize;
}

void array_free(array *arr)
{
  free(arr->data);
  free(arr);
}

list *list_new(void *data, list *next)
{
  list *l = (list*)malloc(sizeof(list));
  l->data = data;
  l->next = next;
  return l;
}

list *list_copy(list *orig, list_copy_t copy)
{
  list *newstart = NULL;
  list **newptr = &newstart;
  while (orig) {
    *newptr = (list*)malloc(sizeof(list));

    if (copy)
      (*newptr)->data = copy(orig->data);
    else
      (*newptr)->data = orig->data;
    (*newptr)->next = NULL;
    newptr = &((*newptr)->next);
    orig = orig->next;
  }
  return newstart;
}

void list_append(list **l, void *data)
{
  list **lptr = l;
  while (*lptr)
    lptr = &((*lptr)->next);
  *lptr = list_new(data,NULL);
}

void list_push(list **l, void *data)
{
  *l = list_new(data,*l);
}

void *list_pop(list **l)
{
  void *data;
  list *next;
  if (NULL == *l)
    return NULL;
  data = (*l)->data;
  next = (*l)->next;
  free(*l);
  *l = next;
  return data;
}

int list_count(list *l)
{
  int count = 0;
  for (; l; l = l->next)
    count++;
  return count;
}

void *list_item(list *l, int item)
{
  while (0 < item) {
    assert(l->next);
    l = l->next;
    item--;
  }
  return l->data;
}

void list_free(list *l, list_d_t d) /* Can be called from native code */
{
  while (l) {
    list *next = l->next;
    if (d)
      d(l->data);
    free(l);
    l = next;
  }
}

int list_contains_string(list *l, const char *str)
{
  for (; l; l = l->next)
    if (((NULL == str) && (NULL == l->data)) ||
        ((NULL != str) && (NULL != l->data) && !strcmp(str,(char*)l->data)))
      return 1;
  return 0;
}

int list_contains_ptr(list *l, const void *data)
{
  for (; l; l = l->next)
    if (l->data == data)
      return 1;
  return 0;
}

void list_remove_ptr(list **l, void *ptr)
{
  list *old;
  assert(list_contains_ptr(*l,ptr));
  while ((*l)->data != ptr)
    l = &((*l)->next);
  old = *l;
  *l = (*l)->next;
  free(old);
}

stack *stack_new(void)
{
  stack *s = (stack*)calloc(1,sizeof(stack));
  s->alloc = 1;
  s->count = 0;
  s->data = (void**)malloc(s->alloc*sizeof(void*));
  return s;
}

void stack_free(stack *s)
{
  free(s->data);
  free(s);
}

void stack_push(stack *s, void *c)
{
  if (s->count == s->alloc) {
    if (s->count >= STACK_LIMIT) {
      fprintf(stderr,"Out of stack space\n");
      exit(1);
    }
    s->alloc *= 2;
    s->data = (void**)realloc(s->data,s->alloc*sizeof(void*));
  }
  s->data[s->count++] = c;
}

static int getsignbit(double d)
{
  char tmp[100];
  sprintf(tmp,"%f",d);
  return ('-' == tmp[0]);
}

void format_double(char *str, int size, double d)
{
  if (isnan(d)) {
    snprintf(str,size,"NaN");
  }
  else if (isinf(d) && (0.0 < d)) {
    snprintf(str,size,"INF");
  }
  else if (isinf(d) && (0.0 > d)) {
    snprintf(str,size,"-INF");
  }
  else if (d == floor(d)) {
    if ((0.0 == d) && (getsignbit(d) && (0.0 == d)))
      snprintf(str,size,"-0");
    else
      snprintf(str,size,"%.0f",d);
  }
  else if ((0.000001 < fabs(d)) && (1000000.0 > fabs(d))) {
    int ipart = (int)d;
    double fraction;
    int start;
    char tmp[100];
    int pos;

    if (0.0 < d)
      fraction = d - floor(d);
    else
      fraction = d - ceil(d);

    start = getsignbit(d) ? 1 : 0;

    sprintf(tmp,"%f",fraction);
    pos = strlen(tmp)-1;
    while ((2+start < pos) && ('0' == tmp[pos]))
      tmp[pos--] = '\0';
    assert('0' == tmp[start]);
    assert('.' == tmp[start+1]);

    snprintf(str,size,"%d.%s",ipart,tmp+start+2);
  }
  else if (0.0 == d) {
    snprintf(str,size,"0");
  }
  else {
    snprintf(str,size,"%f",d);
  }
}

void print_double(FILE *f, double d)
{
  char str[100];
  format_double(str,100,d);
  fprintf(f,"%s",str);
}

void print_escaped(FILE *f, const char *str)
{
  const char *c;
  for (c = str; *c; c++) {
    switch (*c) {
    case '\n':
      fprintf(f,"\\n");
      break;
    case '\r':
      fprintf(f,"\\r");
      break;
    case '\t':
      fprintf(f,"\\t");
      break;
    case '"':
      fprintf(f,"\\\"");
      break;
    default:
      fprintf(f,"%c",*c);
      break;
    }
  }
}

void print_double_escaped(FILE *f, const char *str)
{
  const char *c;
  for (c = str; *c; c++) {
    switch (*c) {
    case '\n':
      fprintf(f,"\\\\n");
      break;
    case '\r':
      fprintf(f,"\\\\r");
      break;
    case '\t':
      fprintf(f,"\\\\t");
      break;
    case '"':
      fprintf(f,"\\\\\\\"");
      break;
    default:
      fprintf(f,"%c",*c);
      break;
    }
  }
}

char *unescape(char *chars)
{
  int len = strlen(chars);
  char *unescaped = (char*)malloc(len+1);
  int from = 0;
  int to = 0;
  while (from < len) {
    if ('\\' == chars[from]) {
      from++;
      switch (chars[from]) {
      case 'b': unescaped[to++] = '\b'; break;
      case 't': unescaped[to++] = '\t'; break;
      case 'n': unescaped[to++] = '\n'; break;
      case 'f': unescaped[to++] = '\f'; break;
      case 'r': unescaped[to++] = '\r'; break;
      case '"': unescaped[to++] = '\"'; break;
      case '\'': unescaped[to++] = '\''; break;
      case '\\': unescaped[to++] = '\\'; break;
      }
      from++;
    }
    else {
      unescaped[to++] = chars[from++];
    }
  }
  unescaped[to] = '\0';
  return unescaped;
}

char *escape(char *chars)
{
  int len = strlen(chars);
  char *escaped = (char*)malloc(len*2+1);
  int from = 0;
  int to = 0;
  while (from < len) {
    switch (chars[from]) {
    case '\b':
      escaped[to++] = '\\';
      escaped[to++] = 'b';
      break;
    case '\t':
      escaped[to++] = '\\';
      escaped[to++] = 't';
      break;
    case '\n':
      escaped[to++] = '\\';
      escaped[to++] = 'n';
      break;
    case '\f':
      escaped[to++] = '\\';
      escaped[to++] = 'f';
      break;
    case '\r':
      escaped[to++] = '\\';
      escaped[to++] = 'r';
      break;
    case '\"':
      escaped[to++] = '\\';
      escaped[to++] = '"';
      break;
    case '\'':
      escaped[to++] = '\\';
      escaped[to++] = '\'';
      break;
    case '\\':
      escaped[to++] = '\\';
      escaped[to++] = '\\';
      break;
    default:
      escaped[to++] = chars[from];
      break;
    }
    from++;
  }
  escaped[to] = '\0';
  return escaped;
}

char *mkstring(const char *data, int len)
{
  char *str = (char*)malloc(len+1);
  memcpy(str,data,len);
  str[len] = '\0';
  return str;
}

char *substring(const char *str, int begin, int end)
{
  int len = strlen(str);
  char *sub;
  assert(begin <= len);
  assert(end <= len);
  assert(begin <= end);
  sub = malloc(end-begin+1);
  memcpy(sub,&str[begin],end-begin);
  sub[end-begin] = '\0';
  return sub;
}

struct timeval timeval_diff(struct timeval from, struct timeval to)
{
  struct timeval diff;
  diff.tv_sec = to.tv_sec - from.tv_sec;
  diff.tv_usec = to.tv_usec - from.tv_usec;
  if (0 > diff.tv_usec) {
    diff.tv_sec -= 1;
    diff.tv_usec += 1000000;
  }
  return diff;
}

int timeval_diffms(struct timeval from, struct timeval to)
{
  struct timeval diff = timeval_diff(from,to);
  return diff.tv_sec*1000 + diff.tv_usec/1000;
}

struct timeval timeval_addms(struct timeval t, int ms)
{
  t.tv_sec += ms/1000;
  t.tv_usec += (ms%1000)*1000;
  if (t.tv_usec >= 1000000) {
    t.tv_usec -= 1000000;
    t.tv_sec++;
  }
  return t;
}

int hash(const void *mem, int size)
{
  int h = 0;
  int i;
  for (i = 0; i < size; i++)
    h += ((char*)mem)[i];
  h %= GLOBAL_HASH_SIZE;
  if (0 > h)
    h += GLOBAL_HASH_SIZE;
  return h;
}

void parse_cmdline(const char *line, int *argc, char ***argv)
{
  const char *c;
  const char *start;
  int argno = 0;
  *argc = 0;

  c = line;
  while (1) {
    if ((('\0' == *c) || isspace(*c)) && ((c > line) && !isspace(*(c-1))))
      (*argc)++;
    if ('\0' == *c)
      break;
    c++;
  }

  *argv = (char**)malloc((*argc)*sizeof(char*));

  start = line;
  c = line;
  while (1) {
    if ((('\0' == *c) || isspace(*c)) && ((c > line) && !isspace(*(c-1)))) {
      (*argv)[argno] = (char*)malloc(c-start+1);
      memcpy((*argv)[argno],start,c-start);
      (*argv)[argno][c-start] = '\0';
      argno++;
    }
    if ('\0' == *c)
      break;
    if (isspace(*c))
      start = c+1;
    c++;
  }
}

void free_args(int argc, char **argv)
{
  int i;
  for (i = 0; i < argc; i++)
    free(argv[i]);
  free(argv);
}

int parse_address(const char *address, char **host, int *port)
{
  const char *colon = strchr(address,':');
  const char *portstr;
  char *end = NULL;

  if (NULL == colon)
    return -1;

  portstr = colon+1;
  *port = strtol(portstr,&end,10);
  if (('\0' == *portstr) || ('\0' != *end))
    return -1;

  *host = (char*)malloc(colon-address+1);
  memcpy(*host,address,colon-address);
  (*host)[colon-address] = '\0';
  return 0;
}

void init_mutex(pthread_mutex_t *mutex)
{
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
  pthread_mutex_t tmp = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
  *mutex = tmp;
#else
  pthread_mutex_init(mutex,NULL);
#endif
}

void destroy_mutex(pthread_mutex_t *mutex)
{
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
  /* no action required */
#else
  pthread_mutex_destroy(mutex);
#endif
}

#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
int check_mutex_locked(pthread_mutex_t *mutex)
{
  if (EDEADLK == pthread_mutex_lock(mutex)) {
    return 1;
  }
  else {
    pthread_mutex_unlock(mutex);
    return 0;
  }
}

int check_mutex_unlocked(pthread_mutex_t *mutex)
{
  if (0 == pthread_mutex_lock(mutex)) {
    pthread_mutex_unlock(mutex);
    return 1;
  }
  else {
    return 0;
  }
}
#else
/* Note: these functions are only used for assertions, so it is safe to return true here if
   we don't have support for error checking mutexes (e.g. on OS X). */
int check_mutex_locked(pthread_mutex_t *mutex)
{
  return 1;
}
int check_mutex_unlocked(pthread_mutex_t *mutex)
{
  return 1;
}
#endif

void lock_mutex(pthread_mutex_t *mutex)
{
  int r = pthread_mutex_lock(mutex);
  assert(0 == r);
}

void unlock_mutex(pthread_mutex_t *mutex)
{
  int r = pthread_mutex_unlock(mutex);
  assert(0 == r);
}

void enable_invalid_fpe()
{
#ifdef HAVE_FEENABLEEXCEPT
  feenableexcept(FE_INVALID);
#else
/*   printf("mac: setting floating point environment, thread = %p\n",pthread_self()); */
  fenv_t env;
  if (0 != fegetenv(&env))
    fatal("fegetenv: %s",strerror(errno));
  env.__control &= ~FE_INVALID;
  env.__mxcsr &= (~FE_INVALID << 7);
  if (0 != fesetenv(&env))
    fatal("fesetenv: %s",strerror(errno));
#endif
}
