/*
 * This file is part of the GridXSLT project
 * Copyright (C) 2005 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#include "util/xmlutils.h"
#include "util/list.h"
#include "util/stringbuf.h"
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <argp.h>

const char *argp_program_version =
  "runtests 0.1";

static char doc[] =
  "Perform a series of regression tests";

static char args_doc[] = "PATH";

static struct argp_option options[] = {
  {"diff",                  'd', 0,      0,  "Print a diff between the expected and actual output"},
  {"v",                     'v', "CMD",  0,  "Run tests through valgrind command CMD and report "
                                             "leaks/errors"},
  { 0 }
};

struct arguments {
  char *path;
  int valgrind;
  char *valgrind_cmd;
  int diff;
};

error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = state->input;

  switch (key) {
  case 'd':
    arguments->diff = 1;
    break;
  case 'v':
    arguments->valgrind = 1;
    arguments->valgrind_cmd = arg;
    break;
  case ARGP_KEY_ARG:
    if (0 == state->arg_num)
      arguments->path = arg;
    else
      argp_usage (state);
    break;
  case ARGP_KEY_END:
    if (1 > state->arg_num)
      argp_usage (state);
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

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

void expand_args(char *program, char *infilename, char ***args)
{
  char *start;
  char *cur;
  int argno = 0;
  char *cmdline = strdup(program);
  int count = 1;
  collapse_whitespace(cmdline);
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
  (*args)[argno++] = infilename;
  (*args)[argno] = NULL;

  free(cmdline);
}

int run_program(char *program, char *infilename, stringbuf *output, int *status)
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
    execlp(program,program,infilename,NULL);
    perror("execlp");
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
    if (!strncmp(entry->d_name,"vglog.pid",strlen("vglog.pid"))) {
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
    if (S_ISDIR(statbuf.st_mode) && strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")) {
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
    pl = calloc(1,sizeof(program_loc));
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
  char *program = NULL;
  stringbuf *indata = stringbuf_new();
  FILE *in;
  int r;
  char buf[1024];
  pid_t pid;
  stringbuf *vgoutput = stringbuf_new();
  char *out_filename = NULL;
  FILE *out = NULL;
  list *output_files = NULL;
  list *l;

  if (!justrun)
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
        if (args->valgrind) {
          stringbuf *temp = stringbuf_new();
          char *realprog = find_program(line);
          stringbuf_format(temp,"%s --log-file="TEMP_DIR"/vglog %s",args->valgrind_cmd,realprog);
          program = strdup(temp->data);
          stringbuf_free(temp);
          free(realprog);
        }
        else {
          program = find_program(line);
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

  if ((STATE_RETURN != state) || !got_expected_rc || (NULL == program)) {
    fprintf(stderr,"Invalid test file\n");
    exit(1);
  }

  if (NULL != output_files)
    pid = run_program(program,NULL,output,&status);
  else
    pid = run_program(program,tempname,output,&status);
  free(program);

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

    if (WIFSIGNALED(status)) {
      printf("%s\n",sys_siglist[WTERMSIG(status)]);
    }
    else if (!WIFEXITED(status)) {
      printf("abnormal termination\n");
    }
    else if (WEXITSTATUS(status) != expected_rc) {
      printf("incorrect exit status %d (expected %d)\n",WEXITSTATUS(status),expected_rc);
    }
    else if ((expected->size != output->size) ||
             memcmp(expected->data,output->data,expected->size-1)) {
      printf("incorrect output\n");
      if (args->diff)
        printdiff(expected,output);
    }
    else if (1 != vgoutput->size) {
      printf("valgrind errors\n");
    }
    else {
      printf("passed\n");
      passed = 1;
    }
  }

  if (1 != vgoutput->size)
    printf("%s",vgoutput->data);



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
  list_free(output_files,free);

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

  if (0 != process_dir_r(testdir,&passes,&failures,args))
    return 1;

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
  int r;

  memset(&arguments,0,sizeof(struct arguments));

  setbuf(stdout,NULL);

  argp_parse (&argp, argc, argv, 0, 0, &arguments);

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

  list_free(program_locs,(void*)program_loc_free);

  return r;
}
