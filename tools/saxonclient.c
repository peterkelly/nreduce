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
 * $Id: saxonclient.cpp 164 2005-11-04 10:59:49Z pmkelly $
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#define SERVLET_URL "http://localhost:8080/saxon/servlet/SaxonServlet"

typedef struct buffer {
  int alloc;
  int size;
  char *data;
} buffer;

char *get_abs_filename(char *rel)
{
  char cwd[PATH_MAX];
  char *full;
  char *absolute = (char*)malloc(PATH_MAX);
  getcwd(cwd,PATH_MAX);

  full = (char*)malloc(strlen(cwd)+1+strlen(rel)+1);
  sprintf(full,"%s/%s",cwd,rel);

  realpath(full,absolute);

  free(full);
  return absolute;
}

size_t write_curl(void *ptr, size_t size, size_t nmemb, void *data)
{
  buffer *buf = (buffer*)data;
  if (buf->size + size*nmemb > buf->alloc) {
    while (buf->size + size*nmemb > buf->alloc)
      buf->alloc *= 2;
    buf->data = (char*)realloc(buf->data,buf->alloc);
  }
  memcpy(&buf->data[buf->size],ptr,size*nmemb);
  buf->size += size*nmemb;
  return size*nmemb;
}

int main(int argc, char **argv)
{
  char *input;
  char *source;
  char *url;
  CURL *h;
  CURLcode cr;
  buffer response;

  setbuf(stdout,NULL);

  if (3 > argc) {
    fprintf(stderr,"Usage: saxonclient <input> <source>\n");
    return 1;
  }

  input = get_abs_filename(argv[1]);
  source = get_abs_filename(argv[2]);
  response.alloc = 1024;
  response.size = 0;
  response.data = (char*)malloc(response.alloc);

  url = (char*)malloc(strlen(SERVLET_URL)+strlen("?source=&input=")+strlen(input)+strlen(source)+1);
  sprintf(url,"%s?source=%s&input=%s",SERVLET_URL,source,input);

  curl_global_init(CURL_GLOBAL_ALL);
  h = curl_easy_init();


  curl_easy_setopt(h,CURLOPT_URL,url);
  curl_easy_setopt(h,CURLOPT_WRITEFUNCTION,write_curl);
  curl_easy_setopt(h,CURLOPT_WRITEDATA,&response);
  curl_easy_setopt(h,CURLOPT_USERAGENT,"libcurl-agent/1.0");
  cr = curl_easy_perform(h);

  if (0 != cr) {
    fprintf(stderr,"%s",curl_easy_strerror(cr));
    exit(1);
  }

  curl_easy_cleanup(h);

  write(STDOUT_FILENO,response.data,response.size);

  free(input);
  free(source);
  free(response.data);
  free(url);

  return 0;
}
