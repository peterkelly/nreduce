/*
 * This file is part of the GridXSLT project
 * Copyright (C) 2005-2009 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>

typedef struct stringbuf {
  int size;
  int allocated;
  char *data;
} stringbuf;

stringbuf *stringbuf_new()
{
  stringbuf *buf = (stringbuf*)malloc(sizeof(stringbuf));
  buf->allocated = 2;
  buf->size = 1;
  buf->data = (char*)malloc(buf->allocated);
  buf->data[0] = '\0';
  return buf;
}

void stringbuf_vformat(stringbuf *buf, const char *format, va_list ap)
{
  assert(0 < buf->size);
  assert(buf->size <= buf->allocated);
  while (1) {
    va_list tmp;
    int r;

    va_copy(tmp,ap);
    r = vsnprintf(&buf->data[buf->size-1],buf->allocated-buf->size,format,ap);
    va_end(tmp);

    if ((0 > r) || (r >= buf->allocated-buf->size)) {
      buf->allocated *= 2;
      buf->data = realloc(buf->data,buf->allocated);
    }
    else {
      buf->size += r;
      break;
    }
  }
  assert(buf->size <= buf->allocated);
  buf->data[buf->size-1] = '\0';
}

void stringbuf_format(stringbuf *buf, const char *format, ...)
{
  va_list ap;

  va_start(ap,format);
  stringbuf_vformat(buf,format,ap);
  va_end(ap);
}

void stringbuf_append(stringbuf *buf, const char *data, int size)
{
  while (buf->size+size >= buf->allocated) {
    buf->allocated *= 2;
    buf->data = (char*)realloc(buf->data,buf->allocated);
  }
  if (NULL != data)
    memcpy(buf->data+buf->size-1,data,size);
  else
    memset(buf->data+buf->size-1,0,size);
  buf->size += size;
  buf->data[buf->size-1] = '\0';
}

void stringbuf_clear(stringbuf *buf)
{
  buf->size = 1;
  buf->data[0] = '\0';
}

void stringbuf_free(stringbuf *buf)
{
  free(buf->data);
  free(buf);
}

typedef struct list {
  void *data;
  struct list *next;
} list;

typedef void (*list_d_t)(void *a);

list *list_new(void *data, list *next)
{
  list *l = (list*)malloc(sizeof(list));
  l->data = data;
  l->next = next;
  return l;
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

void list_free(list *l, list_d_t d)
{
  while (l) {
    list *next = l->next;
    if (d)
      d(l->data);
    free(l);
    l = next;
  }
}

typedef struct progsubst progsubst;

struct progsubst {
  char *name;
  char *prog;
};

static void progsubst_free(progsubst *s)
{
  free(s->name);
  free(s->prog);
}

const char *progsubst_lookup(list *substitutions, const char *name)
{
  list *l;
  for (l = substitutions; l; l = l->next) {
    progsubst *s = (progsubst*)l->data;
    if (!strcmp(s->name,name))
      return s->prog;
  }
  return NULL;
}

struct arguments {
  char *path;
  int inprocess;
  int n;
  int hide_output;
  int valgrind;
  char *valgrind_cmd;
  int diff;
  list *substitutions;
};

static void usage()
{
  printf(
"Usage: runtests [OPTION...] PATH\n"
"Perform a series of regression tests\n"
"\n"
"  -d, --diff                 Print a diff between the expected and actual\n"
"                             output\n"
"  -h, --hide-output          Hide status output\n"
"  -i, --inprocess            Run all tests in-process\n"
"  -n, --repeat N             Run tests n times (dirs only)\n"
"  -s, --substitute NAME=PROG Use PROG as the executable whenever a test\n"
"                             specifies NAME\n"
"  -v, --valgrind CMD         Run tests through valgrind command CMD and report\n"
"                             leaks/errors\n");
  exit(1);
}

static void parse_args(int argc, char **argv, struct arguments *arguments)
{
  int i;
  if (1 >= argc)
    usage();

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i],"-d") || !strcmp(argv[i],"--diff")) {
      arguments->diff = 1;
    }
    else if (!strcmp(argv[i],"-v") || !strcmp(argv[i],"--valgrind")) {
      if (++i >= argc)
        usage();
      arguments->valgrind = 1;
      arguments->valgrind_cmd = argv[i];
    }
    else if (!strcmp(argv[i],"-i") || !strcmp(argv[i],"--inprocess")) {
      arguments->inprocess = 1;
    }
    else if (!strcmp(argv[i],"-n") || !strcmp(argv[i],"--repeat")) {
      if (++i >= argc)
        usage();
      arguments->n = atoi(argv[i]);
    }
    else if (!strcmp(argv[i],"-h") || !strcmp(argv[i],"--hide-output")) {
      arguments->hide_output = 1;
    }
    else if (!strcmp(argv[i],"-s") || !strcmp(argv[i],"--substitute")) {
      progsubst *s;
      char *equals;
      int namelen;
      int proglen;

      if (++i >= argc)
        usage();

      s = (progsubst*)calloc(1,sizeof(progsubst));
      equals = strchr(argv[i],'=');

      if (NULL == equals) {
	fprintf(stderr,"Invalid program substitution: must be of the form NAME=PROG\n");
	exit(1);
      }
      namelen = equals-argv[i];
      proglen = strlen(argv[i])-namelen-1;
      s->name = (char*)malloc(namelen+1);
      s->prog = (char*)malloc(proglen+1);
      memcpy(s->name,argv[i],namelen);
      memcpy(s->prog,equals+1,proglen);
      s->name[namelen] = '\0';
      s->prog[proglen] = '\0';
      list_append(&arguments->substitutions,s);
    }
    else if (NULL == arguments->path) {
      arguments->path = argv[i];
    }
    else {
      usage();
    }
  }
}

#define TEMP_DIR "runtests.tmp"

#define PROGRAM_STR \
        "=================================== PROGRAM ===================================="
#define CMDLINE_STR \
        "============================= COMMAND-LINE ARGUMENTS ==========================="
#define FILE_STR \
        "===================================== FILE ====================================="
#define INPUT_STR \
        "==================================== INPUT ====================================="
#define OUTPUT_STR \
        "==================================== OUTPUT ===================================="
#define RETURN_STR \
        "================================== RETURN CODE ================================="

extern FILE *yyin;
extern int lex_lineno;
int yyparse();

#define STATE_START   0
#define STATE_PROGRAM 1
#define STATE_CMDLINE 2
#define STATE_FILE    3
#define STATE_INPUT   4
#define STATE_OUTPUT  5
#define STATE_RETURN  6

char *collapse_whitespace(const char *orig)
{
  int len = strlen(orig);
  char *str = (char*)malloc(len+1);
  int pos = 0;
  int havespace = 0;
  int i;

  for (i = 0; i < len; i++) {
    if (isspace(orig[i])) {
      if (!havespace && (0 < pos))
        str[pos++] = ' ';
      havespace = 1;
    }
    else {
      str[pos++] = orig[i];
      havespace = 0;
    }
  }
  if ((0 < pos) && (' ' == str[pos-1]))
    pos--;
  str[pos] = '\0';
  return str;
}

void expand_args(char *program, char *infilename, char ***args)
{
  char *start;
  char *cur;
  int argno = 0;
  char *cmdline = collapse_whitespace(program);
  int count = 1;
  for (cur = cmdline; '\0' != *cur; cur++)
    if (' ' == *cur)
      count++;

  (*args) = (char**)malloc((count+2)*sizeof(char*));

  start = cmdline;
  cur = cmdline;
  while (1) {
    if ('\0' == *cur || isspace(*cur)) {
      int endc = *cur;
      *cur = '\0';
      if (cur != start) {
        (*args)[argno++] = strdup(start);
      }
      *cur = endc;
      start = cur+1;
    }
    if ('\0' == *cur)
      break;
    cur++;
  }
  (*args)[argno++] = infilename ? strdup(infilename) : infilename;
  (*args)[argno] = NULL;

  free(cmdline);
}

int run_program_exec(char *program, char *infilename, stringbuf *output, int *status)
{
  int fds[2];
  pid_t pid;
  char **args;

  if (0 > pipe(fds)) {
    perror("pipe");
    exit(1);
  }

  if (0 > (pid = fork())) {
    perror("fork");
    exit(1);
  }
  else if (0 == pid) {
    /* child */
    dup2(fds[1],STDOUT_FILENO);
    dup2(fds[1],STDERR_FILENO);
    close(fds[0]);
    expand_args(program,infilename,&args);
    execvp(args[0],args);
    perror(args[0]);
    exit(-1);
  }
  else {
    /* parent */
    char buf[1024];
    int r;
    close(fds[1]);

    while (0 < (r = read(fds[0],buf,1024)))
      stringbuf_append(output,buf,r);

    close(fds[0]);

    if (0 > waitpid(pid,status,0)) {
      perror("waitpid");
      exit(1);
    }
  }

  return pid;
}

void printdiff(stringbuf *expected, stringbuf *actual)
{
  FILE *f;

  if (NULL == (f = fopen(TEMP_DIR"/expected","w"))) {
    fprintf(stderr,"Can't write to "TEMP_DIR"/expected: %s\n",strerror(errno));
    exit(1);
  }
  fwrite(expected->data,1,expected->size-1,f);
  fclose(f);

  if (NULL == (f = fopen(TEMP_DIR"/actual","w"))) {
    fprintf(stderr,"Can't write to "TEMP_DIR"/actual: %s\n",strerror(errno));
    exit(1);
  }
  fwrite(actual->data,1,actual->size-1,f);
  fclose(f);

  if (-1 == system("diff -up "TEMP_DIR"/expected "TEMP_DIR"/actual")) {
    fprintf(stderr,"system() failed\n");
    exit(1);
  }

  if (0 != unlink(TEMP_DIR"/expected")) {
    fprintf(stderr,"Can't delete "TEMP_DIR"/expected: %s\n",strerror(errno));
    exit(1);
  }

  if (0 != unlink(TEMP_DIR"/actual")) {
    fprintf(stderr,"Can't delete "TEMP_DIR"/actual: %s\n",strerror(errno));
    exit(1);
  }
}

void process_valgrind_log(struct arguments *args, stringbuf *output)
{
  DIR *dir;
  struct dirent *entry;
  char *vglog = NULL;
  FILE *vgf;
  stringbuf *logdata = stringbuf_new();
  char buf[1024];
  int r;
  char *line;
  if (NULL == (dir = opendir(TEMP_DIR))) {
    fprintf(stderr,"Can't open directory %s: %s\n",TEMP_DIR,strerror(errno));
    exit(1);
  }
  while (NULL != (entry = readdir(dir))) {
    if (!strncmp(entry->d_name,"vglog.",strlen("vglog."))) {
      vglog = (char*)malloc(strlen(TEMP_DIR)+1+strlen(entry->d_name)+1);
      sprintf(vglog,"%s/%s",TEMP_DIR,entry->d_name);
    }
  }
  closedir(dir);
  if (NULL == vglog) {
    fprintf(stderr,"Could not find valgrind log file in %s\n",TEMP_DIR);
    exit(1);
  }
  if (NULL == (vgf = fopen(vglog,"r"))) {
    fprintf(stderr,"Can't open valgrind log file %s: %s\n",vglog,strerror(errno));
    exit(1);
  }

  while (0 < (r = fread(buf,1,1024,vgf)))
    stringbuf_append(logdata,buf,r);

  fclose(vgf);
  if (0 != unlink(vglog)) {
    fprintf(stderr,"Could not delete valgrind log file %s: %s\n",vglog,strerror(errno));
    exit(1);
  }
  free(vglog);

  line = logdata->data;
  while (line && '\0' != *line) {
    char *end = strchr(line,'\n');
    char oldend;
    char *line2 = line;
    if (NULL == end)
      end = line+strlen(line);
    oldend = *end;
    *end = '\0';

    while ('=' == *line2)
      line2++;
    while (isdigit(*line2))
      line2++;
    while ('=' == *line2)
      line2++;
    if (' ' == *line2)
      line2++;

    stringbuf_format(output,"    %s\n",line2);

    *end = oldend;
    line = strchr(line,'\n');
    if (line)
      line++;
  }


  stringbuf_free(logdata);
}

char *find_program_r(char *name, char *path)
{
  DIR *dir;
  struct dirent *entry;
  if (NULL == (dir = opendir(path))) {
    perror(path);
    exit(1);
  }
  while (NULL != (entry = readdir(dir))) {
    char *fullname = (char*)malloc(strlen(path)+1+strlen(entry->d_name)+1);
    struct stat statbuf;
    sprintf(fullname,"%s/%s",path,entry->d_name);
    if (0 != stat(fullname,&statbuf)) {
      perror(fullname);
      exit(1);
    }
    if (S_ISDIR(statbuf.st_mode) && strcmp(entry->d_name,".") &&
        strcmp(entry->d_name,"..") && strcmp(entry->d_name,".libs")) {
      char *n;
      if (NULL != (n = find_program_r(name,fullname))) {
        free(fullname);
        closedir(dir);
        return n;
      }
    }
    else if (S_ISREG(statbuf.st_mode) && !strcmp(entry->d_name,name)) {
      closedir(dir);
      return fullname;
    }
    free(fullname);
  }
  closedir(dir);
  return NULL;
}

typedef struct program_loc program_loc;

struct program_loc {
  char *name;
  char *fullname;
};

static list *program_locs = NULL;

void program_loc_free(program_loc *pl)
{
  free(pl->name);
  free(pl->fullname);
  free(pl);
}

char *find_program(char *cmdline)
{
  char *nameend = strchr(cmdline,' ');
  char *name;
  char *fullname = NULL;
  char *newcmdline;
  list *l;
  if (NULL == nameend)
    nameend = cmdline+strlen(cmdline);
  name = (char*)malloc(nameend-cmdline+1);
  memcpy(name,cmdline,nameend-cmdline);
  name[nameend-cmdline] = '\0';

  for (l = program_locs; l; l = l->next) {
    program_loc *pl = (program_loc*)l->data;
    if (!strcmp(pl->name,name))
      fullname = strdup(pl->fullname);
  }

  if (NULL == fullname) {
    program_loc *pl;
    if (NULL == (fullname = find_program_r(name,"."))) {
      fprintf(stderr,"Cannot find program \"%s\"\n",name);
      exit(1);
    }
    pl = (program_loc*)calloc(1,sizeof(program_loc));
    pl->name = strdup(name);
    pl->fullname = strdup(fullname);
    list_push(&program_locs,pl);
  }

  newcmdline = (char*)malloc(strlen(fullname)+strlen(nameend)+1);
  sprintf(newcmdline,"%s%s",fullname,nameend);
  free(fullname);
  free(name);
  return newcmdline;
}

char *subst_program(const char *program, list *substitutions)
{
  char *name = strdup(program);
  char *space = strchr(name,' ');
  char *cmdline;
  const char *realprog;
  int arglen = 0;
  if (NULL != space) {
    int spacepos = space-name;
    arglen = strlen(name)-spacepos;
    *space = '\0';
  }
  realprog = progsubst_lookup(substitutions,name);

  if (NULL == realprog) {
    free(name);
    return NULL;
  }

  cmdline = (char*)malloc(strlen(realprog)+arglen+1);
  if (NULL != space)
    sprintf(cmdline,"%s %s",realprog,space+1);
  else
    sprintf(cmdline,"%s",realprog);
  free(name);
  return cmdline;
}

int test_file(char *filename, char *shortname, int justrun, struct arguments *args)
{
  char *line;
  int state = STATE_START;
  FILE *input;
  char *tempname = TEMP_DIR"/input.xml";
  stringbuf *expected = stringbuf_new();
  stringbuf *output = stringbuf_new();
  int status = 0;
  int got_expected_rc = 0;
  int expected_rc = 0;
  int passed = 0;
  char *cmdline = NULL;
  stringbuf *indata = stringbuf_new();
  FILE *in;
  int r;
  char buf[1024];
  stringbuf *vgoutput = stringbuf_new();
  char *out_filename = NULL;
  FILE *out = NULL;
  list *output_files = NULL;
  list *l;
  int skipsubst = 0;
  int subst = 0;

  if (!justrun && !args->hide_output)
    printf("%-80s... ",filename);

  if (NULL == (in = fopen(filename,"r"))) {
    perror(filename);
    exit(1);
  }

  while (0 < (r = fread(buf,1,1024,in)))
    stringbuf_append(indata,buf,r);

  fclose(in);

  if (NULL == (input = fopen(tempname,"w"))) {
    perror(tempname);
    exit(1);
  }

  line = indata->data;
  while (line) {
    char *end = strchr(line,'\n');
    char oldend;
    if (NULL == end)
      end = line+strlen(line);
    oldend = *end;
    *end = '\0';

    if (!strcmp(line,PROGRAM_STR)) {
      state = STATE_PROGRAM;
    }
    else if (!strcmp(line,CMDLINE_STR)) {
      state = STATE_CMDLINE;
    }
    else if (!strcmp(line,FILE_STR)) {
      out_filename = NULL;
      if (NULL != out)
        fclose(out);
      out = NULL;
      state = STATE_FILE;
    }
    else if (!strcmp(line,INPUT_STR)) {
      state = STATE_INPUT;
    }
    else if (!strcmp(line,OUTPUT_STR)) {
      state = STATE_OUTPUT;
    }
    else if (!strcmp(line,RETURN_STR)) {
      state = STATE_RETURN;
    }
    else {

      switch (state) {
      case STATE_START:
        break;
      case STATE_PROGRAM:

        if (!strcmp(line,"skipsubst")) {
          skipsubst = 1;
        }
        else if (NULL != cmdline) {
          fprintf(stderr,"%s: more than one cmdline specified\n",filename);
          exit(1);
        }
        else if (args->valgrind) {
          stringbuf *temp = stringbuf_new();
          char *realprog = find_program(line);
          stringbuf_format(temp,"%s --log-file="TEMP_DIR"/vglog %s",args->valgrind_cmd,realprog);
          cmdline = strdup(temp->data);
          stringbuf_free(temp);
          free(realprog);
        }
        else if (!args->inprocess) {
          cmdline = subst_program(line,args->substitutions);
          if (NULL == cmdline)
            cmdline = find_program(line);
          else
            subst = 1;
        }
        else {
          cmdline = strdup(line);
        }
        break;
      case STATE_CMDLINE:
        break;
      case STATE_FILE:
        if (NULL != out) {
          fprintf(out,"%s\n",line);
        }
        else {
          filename = (char*)malloc(strlen(TEMP_DIR)+1+strlen(line)+1);
          sprintf(filename,"%s/%s",TEMP_DIR,line);
          if (NULL == (out = fopen(filename,"w"))) {
            fprintf(stderr,"Can't write to %s: %s\n",filename,strerror(errno));
            exit(1);
          }
          list_append(&output_files,filename);
        }
        break;
      case STATE_INPUT:
        fprintf(input,"%s\n",line);
        break;
      case STATE_OUTPUT:
        stringbuf_format(expected,"%s\n",line);
        break;
      case STATE_RETURN:
        if (got_expected_rc) {
          fprintf(stderr,"Invalid test file: already got expected return code\n");
          exit(1);
        }
        expected_rc = atoi(line);
        got_expected_rc = 1;
        break;
      default:
        assert(!"invalid state");
        break;
      }
    }

    *end = oldend;
    line = strchr(line,'\n');
    if (line)
      line++;
    if (line && ('\0' == *line))
      line = NULL;
  }

  if (NULL != out)
    fclose(out);

  fclose(input);

  if ((STATE_RETURN != state) || !got_expected_rc || (NULL == cmdline)) {
    fprintf(stderr,"Invalid test file\n");
    exit(1);
  }

  if (skipsubst && subst && !justrun && !args->hide_output) {
    printf("skipped\n");
    passed = 1;
  }
  else {
    char *infilename = output_files ? NULL : tempname;

    run_program_exec(cmdline,infilename,output,&status);

    /* add trailing newline if necessary */
    if ((2 <= output->size) && ('\n' != output->data[output->size-2]))
      stringbuf_format(output,"\n");
  

    if (args->valgrind)
      process_valgrind_log(args,vgoutput);

    if (justrun) {

      if (args->diff && ((expected->size != output->size) ||
                    memcmp(expected->data,output->data,expected->size-1))) {
        printdiff(expected,output);
      }
      else {
        printf("%s",output->data);
      }
      if (WIFSIGNALED(status)) {
        printf("%s\n",sys_siglist[WTERMSIG(status)]);
      }
      else if (!WIFEXITED(status)) {
        printf("abnormal termination\n");
      }
      else {
        printf("program exited with status %d\n",WEXITSTATUS(status));
      }
    }
    else {

/*       printf("status = %d, WEXITSTATUS = %d\n",status,WEXITSTATUS(status)); */

      if (WIFSIGNALED(status)) {
        if (!args->hide_output)
          printf("%s\n",sys_siglist[WTERMSIG(status)]);
      }
      else if (!WIFEXITED(status)) {
        if (!args->hide_output)
          printf("abnormal termination\n");
      }
      else if (WEXITSTATUS(status) != expected_rc) {
        if (!args->hide_output)
          printf("incorrect exit status %d (expected %d)\n",WEXITSTATUS(status),expected_rc);
      }
      else if ((expected->size != output->size) ||
               memcmp(expected->data,output->data,expected->size-1)) {
        if (!args->hide_output) {
          printf("incorrect output\n");
          if (args->diff)
            printdiff(expected,output);
        }
      }
      else if (1 != vgoutput->size) {
        if (!args->hide_output)
          printf("valgrind errors\n");
      }
      else {
        if (!args->hide_output)
          printf("passed\n");
        passed = 1;
      }
    }

    if (1 != vgoutput->size) {
      printf("%s",vgoutput->data);
    }
  }


  free(cmdline);

/*   printf("got %d bytes of output\n",output->size-1); */
/*   printf("output: (%d) ***%s***\n",output->size-1,output->data); */
/*   printf("expected: (%d) ***%s***\n",expected->size-1,expected->data); */


  stringbuf_free(output);

  stringbuf_free(expected);
  stringbuf_free(indata);

  if (0 != unlink(tempname)) {
    fprintf(stderr,"Can't delete %s: %s\n",tempname,strerror(errno));
    exit(1);
  }

  for (l = output_files; l; l = l->next) {
    out_filename = (char*)l->data;
    if (0 != unlink(out_filename)) {
      fprintf(stderr,"Can't delete %s: %s\n",out_filename,strerror(errno));
      exit(1);
    }
  }
  list_free(output_files,(list_d_t)free);

  stringbuf_free(vgoutput);

  return passed;
}

int process_file(char *filename, struct arguments *args)
{
  return test_file(filename,filename,1,args);
}

int name_compar(const void *a, const void *b)
{
  return strcmp(*((const char**)a),*((const char**)b));
}

int process_dir_r(char *testdir, int *passes, int *failures, struct arguments *args)
{
  DIR *dir;
  struct dirent *entry;
  struct stat statbuf;
  int count = 0;
  char **names;
  int i;

  if (NULL == (dir = opendir(testdir))) {
    perror(testdir);
    exit(1);
  }

  while (NULL != (entry = readdir(dir)))
    count++;
  rewinddir(dir);

  i = 0;
  names = (char**)malloc(count*sizeof(char*));
  while (NULL != (entry = readdir(dir)))
    names[i++] = strdup(entry->d_name);

  qsort(names,count,sizeof(char*),name_compar);

  /* process files */
  for (i = 0; i < count; i++) {
    char *path = (char*)malloc(strlen(testdir)+1+strlen(names[i])+1);
    sprintf(path,"%s/%s",testdir,names[i]);
    if (0 != stat(path,&statbuf)) {
      perror(path);
      exit(1);
    }
    if (S_ISREG(statbuf.st_mode) &&
        (5 <= strlen(names[i])) &&
        !strcmp(names[i]+strlen(names[i])-5,".test")) {
      if (test_file(path,names[i],0,args))
        (*passes)++;
      else
        (*failures)++;
    }
    free(path);
  }

  /* process dirs */
  for (i = 0; i < count; i++) {
    char *path = (char*)malloc(strlen(testdir)+1+strlen(names[i])+1);
    sprintf(path,"%s/%s",testdir,names[i]);
    if (0 != stat(path,&statbuf)) {
      perror(path);
      exit(1);
    }
    if (S_ISDIR(statbuf.st_mode) && strcmp(names[i],".") && strcmp(names[i],"..")) {
      if (0 != process_dir_r(path,passes,failures,args))
        return 1;
    }
    free(path);
    free(names[i]);
  }


  free(names);
  closedir(dir);
  return 0;
}

int process_dir(char *testdir, struct arguments *args)
{
  int passes = 0;
  int failures = 0;

  while (0 < args->n--) {
    if (0 != process_dir_r(testdir,&passes,&failures,args))
      return 1;
  }

  printf("Passes: %d\n",passes);
  printf("Failures: %d\n",failures);

  if (0 == failures)
    return 0;
  else
    return 1;
}

void clear_tempdir()
{
  DIR *dir;
  struct dirent *entry;
  struct stat statbuf;

  if (NULL == (dir = opendir(TEMP_DIR)))
    return;

  while (NULL != (entry = readdir(dir))) {
    char *path = (char*)malloc(strlen(TEMP_DIR)+1+strlen(entry->d_name)+1);
    sprintf(path,"%s/%s",TEMP_DIR,entry->d_name);
    if (0 != stat(path,&statbuf)) {
      fprintf(stderr,"Can't stat %s: %s\n",path,strerror(errno));
      exit(1);
    }
    if (S_ISREG(statbuf.st_mode)) {
      if (0 != unlink(path)) {
        fprintf(stderr,"Can't delete %s: %s\n",path,strerror(errno));
        exit(1);
      }
    }
    free(path);
  }

  closedir(dir);

  if (0 != rmdir(TEMP_DIR)) {
    fprintf(stderr,"Can't remove directory %s: %s\n",TEMP_DIR,strerror(errno));
    exit(1);
  }
}

int main(int argc, char **argv)
{
  struct stat statbuf;
  struct arguments arguments;
  char *pathend;
  int r = 0;

  memset(&arguments,0,sizeof(struct arguments));
  arguments.n = 1;

  setbuf(stdout,NULL);

  parse_args(argc,argv,&arguments);

  pathend = &arguments.path[strlen(arguments.path)-1];
  while ((pathend >= arguments.path) && ('/' == *pathend))
    (*pathend--) = '\0';

  if (0 != stat(arguments.path,&statbuf)) {
    perror(arguments.path);
    exit(1);
  }

  clear_tempdir();

  if (0 != mkdir(TEMP_DIR,0770)) {
    fprintf(stderr,"Can't create directory %s: %s\n",TEMP_DIR,strerror(errno));
    exit(1);
  }

  if (S_ISREG(statbuf.st_mode))
    r = process_file(arguments.path,&arguments);
  else
    r = process_dir(arguments.path,&arguments);

  if (0 != rmdir(TEMP_DIR)) {
    fprintf(stderr,"Can't remove directory %s: %s\n",TEMP_DIR,strerror(errno));
    exit(1);
  }

  list_free(program_locs,(list_d_t)program_loc_free);
  list_free(arguments.substitutions,(list_d_t)progsubst_free);

  return r;
}
