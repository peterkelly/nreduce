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

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <argp.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libxml/xmlwriter.h>
#include <libxml/xmlIO.h>
#include "util/xmlutils.h"

using namespace GridXSLT;

/* const char *argp_program_version = */
/*   "fsxml 0.1"; */

static char doc[] =
  "Produce XML representation of a directory tree";

static char args_doc[] = "PATH";

static struct argp_option options[] = {
  {"permissions",           'p', 0,      0,  "Include access permission info" },
  { 0 }
};

struct arguments {
  int perms;
  char *path;
};

error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = (struct arguments*)state->input;

  switch (key) {
  case 'p':
    arguments->perms = 1;
    break;
  case ARGP_KEY_ARG:
    if (1 <= state->arg_num)
      argp_usage (state);
    arguments->path = arg;
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

void process_dir(xmlTextWriter *writer, char *path, int perms)
{
  DIR *dir;
  struct dirent *entry;

  if (NULL == (dir = opendir(path))) {
    if (EACCES == errno)
      return; /* skip dirs we can't access */
    perror(path);
    exit(1);
  }

  while (NULL != (entry = readdir(dir))) {
    if (strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")) {
      char *fullname = (char*)malloc(strlen(path)+1+strlen(entry->d_name)+1);
      struct stat statbuf;
      sprintf(fullname,"%s/%s",path,entry->d_name);
      if (0 == lstat(fullname,&statbuf)) {
        if (S_ISLNK(statbuf.st_mode)) {
          char dest[PATH_MAX+1];
          int destlen;
          if (0 <= (destlen = readlink(fullname,dest,PATH_MAX))) {
            dest[destlen] = '\0';
            xmlTextWriterStartElement(writer,"link");
            XMLWriter::formatAttribute(writer,"name","%s",entry->d_name);
            XMLWriter::formatAttribute(writer,"dest","%s",dest);
            xmlTextWriterEndElement(writer);
          }
          else if (EACCES != errno) {  /* skip links we can't access */
            perror(fullname);
            exit(1);
          }
        }
        else {

          if (S_ISDIR(statbuf.st_mode)) {
            xmlTextWriterStartElement(writer,"dir");
          }
          else {
            xmlTextWriterStartElement(writer,"file");
          }

          XMLWriter::formatAttribute(writer,"name","%s",entry->d_name);

          if (S_ISREG(statbuf.st_mode))
            XMLWriter::formatAttribute(writer,"size","%d",statbuf.st_size);

          XMLWriter::formatAttribute(writer,"mtime","%d",statbuf.st_mtime);

          if (perms) {
            struct passwd *user;
            struct group *group;


            xmlTextWriterStartElement(writer,"permissions");

            errno = 0;
            user = getpwuid(statbuf.st_uid);
            if (0 != errno) {
              perror("getpwuid");
              exit(1);
            }
            if (NULL == user)
              XMLWriter::formatAttribute(writer,"uid","%d",statbuf.st_uid);
            else
              XMLWriter::formatAttribute(writer,"uid","%s",user->pw_name);

            errno = 0;
            group = getgrgid(statbuf.st_gid);
            if (0 != errno) {
              perror("getgrgid");
              exit(1);
            }
            if (NULL == group)
              XMLWriter::formatAttribute(writer,"gid","%d",statbuf.st_gid);
            else
              XMLWriter::formatAttribute(writer,"gid","%s",group->gr_name);




            xmlTextWriterStartElement(writer,"user");
            XMLWriter::formatAttribute(writer,"read","%d",
                                              statbuf.st_mode & S_IRUSR ? 1 : 0);
            XMLWriter::formatAttribute(writer,"write","%d",
                                              statbuf.st_mode & S_IWUSR ? 1 : 0);
            XMLWriter::formatAttribute(writer,"execute","%d",
                                              statbuf.st_mode & S_IXUSR ? 1 : 0);
            xmlTextWriterEndElement(writer);


            xmlTextWriterStartElement(writer,"group");
            XMLWriter::formatAttribute(writer,"read","%d",
                                              statbuf.st_mode & S_IRGRP ? 1 : 0);
            XMLWriter::formatAttribute(writer,"write","%d",
                                              statbuf.st_mode & S_IWGRP ? 1 : 0);
            XMLWriter::formatAttribute(writer,"execute","%d",
                                              statbuf.st_mode & S_IXGRP ? 1 : 0);
            xmlTextWriterEndElement(writer);

            xmlTextWriterStartElement(writer,"others");
            XMLWriter::formatAttribute(writer,"read","%d",
                                              statbuf.st_mode & S_IROTH ? 1 : 0);
            XMLWriter::formatAttribute(writer,"write","%d",
                                              statbuf.st_mode & S_IWOTH ? 1 : 0);
            XMLWriter::formatAttribute(writer,"execute","%d",
                                              statbuf.st_mode & S_IXOTH ? 1 : 0);
            xmlTextWriterEndElement(writer);


            xmlTextWriterEndElement(writer);
          }

          if (S_ISDIR(statbuf.st_mode))
            process_dir(writer,fullname,perms);
          xmlTextWriterEndElement(writer);

        }

      }
      else if (EACCES != errno) {  /* skip dirs/files we can't access */
        perror(fullname);
        exit(1);
      }
      free(fullname);
    }
  }

  closedir(dir);
}

int main(int argc, char **argv)
{
  struct arguments arguments;
  xmlOutputBuffer *buf = xmlOutputBufferCreateFile(stdout,NULL);
  xmlTextWriter *writer = xmlNewTextWriter(buf);

  setbuf(stdout,NULL);

  memset(&arguments,0,sizeof(arguments));
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  xmlTextWriterSetIndent(writer,1);
  xmlTextWriterSetIndentString(writer,"  ");
  xmlTextWriterStartDocument(writer,NULL,NULL,NULL);
  xmlTextWriterStartElement(writer,"filesystem");

  process_dir(writer,arguments.path,arguments.perms);

  xmlTextWriterEndElement(writer);
  xmlTextWriterEndDocument(writer);
  xmlTextWriterFlush(writer);
  xmlFreeTextWriter(writer);

  return 0;
}
