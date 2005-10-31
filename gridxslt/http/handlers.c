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
 */

#include "handlers.h"
#include "message.h"
#include "http.h"
#include "util/network.h"
#include "util/debug.h"
#include "util/String.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace GridXSLT;

typedef struct file_data file_data;
typedef struct entryinfo entryinfo;
typedef struct tree_data tree_data;
typedef struct tree_node tree_node;

struct file_data {
  int fd;
};

static int file_write(urihandler *uh, msgwriter *mw)
{
  file_data *d = (file_data*)uh->d;
  int r;
  char buf[BUFSIZE];

  if (0 == (r = read(d->fd,buf,BUFSIZE)))
    return 0;

  msgwriter_write(mw,buf,r);
  return 1;
}

static void file_cleanup(urihandler *uh)
{
  file_data *d = (file_data*)uh->d;
  close(d->fd);
  free(d);
}

urihandler *handle_file(const char *uri, msgwriter *mw, int size)
{
  int fd;

  if (0 > (fd = open(uri,O_RDONLY))) {
    error_response(mw,500,"Could not open file %s",uri);
    return NULL;
  }
  else {
    urihandler *uh = (urihandler*)calloc(1,sizeof(urihandler));
    file_data *d = (file_data*)calloc(1,sizeof(file_data));

    message("FILE: %s\n",uri);

    d->fd = fd;
    uh->d = d;
    uh->write = file_write;
    uh->cleanup = file_cleanup;

    msgwriter_response(mw,200,size);
    add_connection_header(mw);

    return uh;
  }
}

struct entryinfo {
  char *name;
  struct stat statbuf;
};

int entryinfo_compar(const void *a, const void *b)
{
  const entryinfo *eia = (const entryinfo*)a;
  const entryinfo *eib = (const entryinfo*)b;
  if (S_ISDIR(eia->statbuf.st_mode) && !S_ISDIR(eib->statbuf.st_mode))
    return -1;
  else if (!S_ISDIR(eia->statbuf.st_mode) && S_ISDIR(eib->statbuf.st_mode))
    return 1;
  return strcasecmp(eia->name,eib->name);
}

void dir_listing(msgwriter *mw, const char *path, DIR *dir)
{
  entryinfo *infos = NULL;
  int infocount = 0;
  int infoalloc = 0;
  struct dirent *entry;
  int i;

  while (NULL != (entry = readdir(dir))) {
    if (strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")) {
      char *fullpath = (char*)malloc(strlen(path)+1+strlen(entry->d_name)+1);
      sprintf(fullpath,"%s/%s",path,entry->d_name);
      if (infocount >= infoalloc) {
        infoalloc += 16;
        infos = (entryinfo*)realloc(infos,infoalloc*sizeof(entryinfo));
      }
      infos[infocount].name = strdup(entry->d_name);
      if (0 != stat(fullpath,&infos[infocount].statbuf))
        memset(&infos[infocount].statbuf,0,sizeof(struct stat));
      infocount++;
      free(fullpath);
    }
  }

  qsort(infos,infocount,sizeof(entryinfo),entryinfo_compar);

  msgwriter_format(mw,"<html>\n"
                   "  <head>\n"
                   "    <title>Directory listing: %s</title>\n"
                   "  </head>\n"
                   "  <body>\n"
                   "    <h1>Directory listing: %s</h1>\n",path,path);
  msgwriter_format(mw,"    <table width=\"100%%\">\n"
                   "      <tr>\n"
                   "        <td><pre>      Name</pre></td>\n"
                   "        <td align=\"right\"><pre>Size    </pre></td>\n"
                   "        <td><pre>Last modified</pre></td>\n"
                   "      </tr>\n"
                   "      <tr>\n"
                   "      </tr>\n");

  for (i = 0; i < infocount; i++) {
    char sizestr[100];
    if (S_ISDIR(infos[i].statbuf.st_mode))
      sprintf(sizestr,"-");
    else
      sprintf(sizestr,"%ld",infos[i].statbuf.st_size);
    msgwriter_format(mw,"    <tr>\n"
                     "      <td><pre>%s<a href=\"%s\">%s</a></pre></td>\n"
                     "      <td align=\"right\"><pre>%s    </pre></td>\n"
                     "      <td><pre>%s</pre></td>\n"
                     "    </tr>\n",
                     S_ISDIR(infos[i].statbuf.st_mode) ? "[DIR] " : "      ",
                     infos[i].name,
                     infos[i].name,
                     sizestr,
                     ctime(&infos[i].statbuf.st_mtime));
  }

  msgwriter_format(mw,"    </table>\n"
                   "  </body>\n"
                   "</html>\n");

}

urihandler *handle_dir(const char *uri, msgwriter *mw)
{
  DIR *dir;
  if ((0 == strlen(uri)) || ('/' != uri[strlen(uri)-1])) {
    char *location = (char*)alloca(strlen(SERVER_URL)+strlen(uri)+2);
    sprintf(location,"%s%s/",SERVER_URL,uri);
    msgwriter_response(mw,301,0);
    add_connection_header(mw);
    msgwriter_header(mw,"Location",location);
    msgwriter_end(mw);
  }
  else if (NULL == (dir = opendir(uri))) {
    error_response(mw,500,"Could not open directory %s",uri);
  }
  else {
    message("DIR: %s\n",uri);

    msgwriter_response(mw,200,-1);
    add_connection_header(mw);
    msgwriter_header(mw,"Content-Type","text/html");

    dir_listing(mw,uri,dir);
    msgwriter_end(mw);
    closedir(dir);
  }

  return NULL;
}

struct tree_node {
  char *fullpath;
  DIR *dir;
};

struct tree_data {
  list *dirs;
};

void tree_node_free(tree_node *tn)
{
  free(tn->fullpath);
  closedir(tn->dir);
  free(tn);
}

int tree_write(urihandler *uh, msgwriter *mw)
{
  tree_data *d = (tree_data*)uh->d;
  tree_node *tn;
  struct dirent *entry;

  ASSERT(!mw->finished);
  ASSERT(NULL != d->dirs);

  tn = (tree_node*)d->dirs->data;

  while (NULL == (entry = readdir(tn->dir))) {

    if (NULL != d->dirs->next) {
      list *old = d->dirs;
      tree_node_free(tn);
      d->dirs = d->dirs->next;
      free(old);
      tn = (tree_node*)d->dirs->data;
    }
    else {
      msgwriter_format(mw,"    </pre>\n"
                        "  </body>\n"
                        "</html>\n");
      return 0;
    }
  }

  debug("dir_writer: %s\n",entry->d_name);
  if (strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")) {
    char *fullpath = (char*)alloca(strlen(tn->fullpath)+1+strlen(entry->d_name)+1);
    struct stat statbuf;
    DIR *childdir;

    sprintf(fullpath,"%s/%s",tn->fullpath,entry->d_name);

    if (0 == stat(fullpath,&statbuf)) {
      msgwriter_format(mw,"%s\n",fullpath);
      if (S_ISDIR(statbuf.st_mode) && (NULL != (childdir = opendir(fullpath)))) {
        tree_node *tn = (tree_node*)calloc(1,sizeof(tree_node));
        tn->fullpath = strdup(fullpath);
        tn->dir = childdir;
        list_push(&d->dirs,tn);
      }
    }

  }

  return 1;
}

void tree_cleanup(urihandler *uh)
{
  tree_data *d = (tree_data*)uh->d;
  list_free(d->dirs,(list_d_t)tree_node_free);
  free(d);
}

urihandler *handle_tree(const char *uri, msgwriter *mw)
{
  DIR *dir;
  if (NULL == (dir = opendir(uri))) {
    error_response(mw,500,"Could not open directory %s",uri);
    return NULL;
  }
  else {
    tree_data *d = (tree_data*)calloc(1,sizeof(tree_data));
    tree_node *tn = (tree_node*)calloc(1,sizeof(tree_node));
    urihandler *uh = (urihandler*)calloc(1,sizeof(urihandler));
    message("TREE: %s\n",uri);
    tn->fullpath = strdup(uri);
    tn->fullpath[strlen(tn->fullpath)-1] = '\0';
    tn->dir = dir;
    list_push(&d->dirs,tn);


    uh->d = d;
    uh->write = tree_write;
    uh->cleanup = tree_cleanup;

    msgwriter_response(mw,200,-1);
    add_connection_header(mw);
    msgwriter_header(mw,"Content-Type","text/html");

    msgwriter_format(mw,"<html>\n"
                        "  <head>\n"
                        "    <title>Tree:: %s</title>\n"
                        "  </head>\n"
                        "  <body>\n"
                        "    <h1>Tree: %s</h1>\n"
                        "    <pre>\n",uri,uri);
    debug("initialized tree handler\n");
    return uh;
  }
}
