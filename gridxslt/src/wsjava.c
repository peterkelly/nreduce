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

#include "xslt/parse.h"
#include "util/namespace.h"
#include "util/stringbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <libxml/xmlwriter.h>
#include <libxml/xmlIO.h>
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#define WSCOMPILE_CONFIG_NAMESPACE "http://java.sun.com/xml/ns/jax-rpc/ri/config"

size_t write_wsdl(void *ptr, size_t size, size_t nmemb, void *data)
{
  stringbuf_append((stringbuf*)data,ptr,size*nmemb);
  return size*nmemb;
}

char *get_porttype(const char *wsdl, char **service_name, char **service_url)
{
  stringbuf *buf = stringbuf_new();
  CURL *h;
  xmlDocPtr doc;
  xmlNodePtr root;
  xmlNodePtr c;
  xmlNodePtr c2;
  xmlNodePtr c3;
  char *porttype = NULL;
  int found = 0;
  CURLcode cr;

  *service_name = NULL;
  *service_url = NULL;

  /* FIXME: error handler for if it can't access the URL */
  curl_global_init(CURL_GLOBAL_ALL);
  h = curl_easy_init();
  curl_easy_setopt(h,CURLOPT_URL,wsdl);
  curl_easy_setopt(h,CURLOPT_WRITEFUNCTION,write_wsdl);
  curl_easy_setopt(h,CURLOPT_WRITEDATA,buf);
  curl_easy_setopt(h,CURLOPT_USERAGENT,"libcurl-agent/1.0");
  cr = curl_easy_perform(h);
  curl_easy_cleanup(h);

  if (0 != cr) {
    fprintf(stderr,"%s: %s\n",wsdl,curl_easy_strerror(cr));
    stringbuf_free(buf);
    return NULL;
  }

  if (NULL == (doc = xmlReadMemory(buf->data,buf->size-1,wsdl,NULL,0))) {
    fprintf(stderr,"%s: XML parse error\n",wsdl);
  }
  else if (NULL == (root = xmlDocGetRootElement(doc))) {
    fprintf(stderr,"%s: No root element\n",wsdl);
  }
  else if (!check_element(root,"definitions",WSDL_NAMESPACE)) {
    fprintf(stderr,"%s: Invalid root element\n",wsdl);
  }
  else {
    for (c = root->children; c; c = c->next) {
      if (check_element(c,"portType",WSDL_NAMESPACE)) {
        if (NULL == (porttype = xmlGetProp(c,"name"))) {
          fprintf(stderr,"%s: no \"name\" attribute in <portType> element\n",wsdl);
          break;
        }
        porttype = strdup(porttype);
        found = 1;
      }
      else if (check_element(c,"service",WSDL_NAMESPACE)) {
        if (NULL == xmlGetProp(c,"name")) {
          fprintf(stderr,"%s: no \"name\" attribute in <service> element\n",wsdl);
          free(porttype);
          porttype = NULL;
          break;
        }
        *service_name = strdup(xmlGetProp(c,"name"));
        for (c2 = c->children; c2; c2 = c2->next) {
          if (check_element(c2,"port",WSDL_NAMESPACE)) {
            for (c3 = c2->children; c3; c3 = c3->next) {
              if (check_element(c3,"address",SOAP_NAMESPACE) &&
                  (NULL != xmlGetProp(c3,"location"))) {
                *service_url = strdup(xmlGetProp(c3,"location"));
              }
            }
          }
        }
      }
    }
  }


  if (!found) {
    fprintf(stderr,"%s: no port type found\n",wsdl);
  }
  else if (NULL == *service_url) {
    fprintf(stderr,"%s: no SOAP binding found\n",wsdl);
    free(porttype);
    porttype = NULL;
  }

  if (NULL != doc)
    xmlFreeDoc(doc);
  stringbuf_free(buf);
  return porttype;
}

int gen_java_bindings(const char *wsdl, const char *pkgname)
{
  int fd;
  xmlOutputBuffer *buf;
  xmlTextWriter *writer;
  char *fn = strdup("/tmp/wsjavaXXXXXX");
  pid_t pid;

  printf("Generating bindings for %s...",wsdl);

  /* create config file for wscompile */
  if (0 > (fd = mkstemp(fn))) {
    perror(fn);
    free(fn);
    return -1;
  }

  buf = xmlOutputBufferCreateFd(fd,NULL);
  writer = xmlNewTextWriter(buf);

  xmlTextWriterSetIndent(writer,1);
  xmlTextWriterSetIndentString(writer,"  ");
  xmlTextWriterStartDocument(writer,NULL,NULL,NULL);
  xmlTextWriterStartElement(writer,"configuration");
  xmlTextWriterWriteAttribute(writer,"xmlns",WSCOMPILE_CONFIG_NAMESPACE);
  xmlTextWriterStartElement(writer,"wsdl");

  xmlTextWriterWriteAttribute(writer,"location",wsdl);
  xmlTextWriterWriteAttribute(writer,"packageName",pkgname);

  xmlTextWriterEndElement(writer);
  xmlTextWriterEndElement(writer);
  xmlTextWriterEndDocument(writer);
  xmlTextWriterFlush(writer);
  xmlFreeTextWriter(writer);
  close(fd);

  /* run wscompile */
  if (0 > (pid = fork())) {
    perror("fork");
    free(fn);
    return -1;
  }
  else if (0 == pid) {
    /* in child */
    execlp("wscompile","wscompile","-keep","-gen",fn,NULL);
    perror("execlp");
    exit(-1);
  }
  else {
    /* in parent */
    int status;
    free(fn);
    if (0 > waitpid(pid,&status,0)) {
      perror("waitpid");
      return -1;
    }
    if (!WIFEXITED(status) || (0 != WEXITSTATUS(status))) {
      fprintf(stderr,"Running wscompile failed.\n");
      return -1;
    }
    printf("done\n");
  }

  return 0;
}

int compile_java_source(char *sourcepath)
{
  pid_t pid;

  printf("Compiling %s...",sourcepath);

  if (0 > (pid = fork())) {
    perror("fork");
    return -1;
  }
  else if (0 == pid) {
    /* in child */
    execlp("javac","javac",sourcepath,NULL);
    perror("execlp");
    exit(-1);
  }
  else {
    /* in parent */
    int status;
    if (0 > waitpid(pid,&status,0)) {
      perror("waitpid");
      return -1;
    }
    if (!WIFEXITED(status) || (0 != WEXITSTATUS(status))) {
      fprintf(stderr,"Running javac failed.\n");
      return -1;
    }
    printf("done\n");
  }

  return 0;
}

int main2(int argc, char **argv)
{
  char *wsdl;
  char *pkgname;

  setbuf(stdout,NULL);

  if (3 > argc) {
    fprintf(stderr,"Usage: wsjava <wsdl> <pkgname>\n");
    exit(1);
  }

  wsdl = argv[1];
  pkgname = argv[2];

  if (0 != gen_java_bindings(wsdl,pkgname))
    return -1;

  return 0;
}

void fix_expr_function_calls(xp_expr *e)
{
  if (e->left)
    fix_expr_function_calls(e->left);
  if (e->right)
    fix_expr_function_calls(e->right);
  if (e->conditional)
    fix_expr_function_calls(e->conditional);

  if (XPATH_EXPR_FUNCTION_CALL == e->type) {
/* FIXME */
#if 0
    xp_expr *stubcall = xp_expr_new(XPATH_EXPR_FUNCTION_CALL,NULL,NULL);
    xp_expr *stubparam = xp_expr_new(XPATH_EXPR_ACTUAL_PARAM,NULL,NULL);
    stubcall->qname.prefix = (char*)malloc(strlen(e->qname.prefix)+strlen("stub")+1);
    sprintf(stubcall->qname.prefix,"%sstub",e->qname.prefix);
    stubcall->qname.localpart = strdup("getStub");
    stubparam->left = stubcall;
    stubparam->nextseq = e->left;
    e->left = stubparam;
#endif
  }

}

void fix_stmt_function_calls(xl_snode *sn)
{
  xl_snode *c;

  if (sn->select)
    fix_expr_function_calls(sn->select);
  if (sn->expr1)
    fix_expr_function_calls(sn->expr1);
  if (sn->expr2)
    fix_expr_function_calls(sn->expr2);
  if (sn->name_expr)
    fix_expr_function_calls(sn->name_expr);

  for (c = sn->param; c; c = c->next)
    fix_stmt_function_calls(c);
  for (c = sn->sort; c; c = c->next)
    fix_stmt_function_calls(c);
  for (c = sn->child; c; c = c->next)
    fix_stmt_function_calls(c);
}

int process_xslt(xl_snode *sroot)
{
  ns_def *def;
  int r = 0;
  int defcount = 0;
  int defno;
  ns_def **wsdl_defs;
  list *l;

  for (l = sroot->namespaces->defs; l && (0 == r); l = l->next) {
    def = (ns_def*)l->data;
    if (def->prefix && !strncmp(def->href,"wsdl:",5)) {
      char *pkgname = (char*)malloc(strlen("wsbinding.")+strlen(def->prefix)+1);
      sprintf(pkgname,"wsbinding.%s",def->prefix);
      if (0 != gen_java_bindings(def->href+5,pkgname))
        r = -1;
      defcount++;
    }
  }

  wsdl_defs = (ns_def**)malloc(defcount*sizeof(ns_def*));
  defno = 0;

  for (l = sroot->namespaces->defs; l && (0 == r); l = l->next) {
    def = (ns_def*)l->data;
    if (def->prefix && !strncmp(def->href,"wsdl:",5))
      wsdl_defs[defno++] = def;
  }

  for (defno = 0; (defno < defcount) && (0 == r); defno++) {
    char *prefix = wsdl_defs[defno]->prefix;
    char *stubprefix = (char*)malloc(strlen(prefix+strlen("stub")+1));
    char *stubhref = (char*)malloc(strlen("java:wsbinding..XSLTStub")+
                                   strlen(prefix)+1);
    char *porttype;
    char *service_name = NULL;
    char *service_url = NULL;

    char *sourcepath = (char*)malloc(strlen(prefix)+strlen("wsbinding//XSLTStub.java")+1);
    sprintf(sourcepath,"wsbinding/%s/XSLTStub.java",prefix);

    printf("Generating %s...",sourcepath);

    sprintf(stubprefix,"%sstub",prefix);
    sprintf(stubhref,"java:wsbinding.%s.XSLTStub",prefix);
    ns_add(sroot->namespaces,stubhref,stubprefix);
    free(stubprefix);
    free(stubhref);

    if (NULL == (porttype = get_porttype(wsdl_defs[defno]->href+5,&service_name,&service_url))) {
      r = -1;
    }
    else {
      FILE *f;

      free(wsdl_defs[defno]->href);
      wsdl_defs[defno]->href = (char*)malloc(strlen("java:wsbinding..")+
                                             strlen(prefix)+strlen(porttype)+1);
      sprintf(wsdl_defs[defno]->href,"java:wsbinding.%s.%s",prefix,porttype);


      if (NULL == (f = fopen(sourcepath,"w"))) {
        perror(sourcepath);
        r = -1;
      }
      else {
        fprintf(f,"package wsbinding.%s;\n",prefix);
        fprintf(f,"\n");
        fprintf(f,"import javax.xml.rpc.Stub;\n");
        fprintf(f,"\n");
        fprintf(f,"public class XSLTStub\n");
        fprintf(f,"{\n");
        fprintf(f,"  private static %s instance = null;\n",porttype);
        fprintf(f,"  private static final String URL =\n");
        fprintf(f,"    \"%s\";\n",service_url);
        fprintf(f,"\n");
        fprintf(f,"  public static %s getStub()\n",porttype);
        fprintf(f,"  {\n");
        fprintf(f,"    if (instance == null) {\n");
        fprintf(f,"      Stub stub = (Stub)(new %s_Impl().get%sPort());\n",service_name,porttype);
        fprintf(f,"      stub._setProperty(javax.xml.rpc.Stub.ENDPOINT_ADDRESS_PROPERTY,URL);\n");
        fprintf(f,"      instance = (%s)stub;\n",porttype);
        fprintf(f,"    }\n");
        fprintf(f,"    return instance;\n");
        fprintf(f,"  }\n");
        fprintf(f,"}\n");

        fclose(f);
        printf("done\n");

        if (0 != compile_java_source(sourcepath))
          r = -1;
      }
      free(porttype);
      free(service_name);
      free(service_url);
    }
    free(sourcepath);

  }

  fix_stmt_function_calls(sroot);

  free(wsdl_defs);
  return r;
}

int main(int argc, char **argv)
{
  char *xsltin;
  char *xsltout;
  FILE *in;
  FILE *out;
  xl_snode *sroot;
  int r = 0;
  error_info ei;

  setbuf(stdout,NULL);

  if (3 > argc) {
    fprintf(stderr,"Usage: wsjava <xsltin> <xsltout>\n");
    exit(1);
  }

  xsltin = argv[1];
  xsltout = argv[2];

  if (NULL == (in = fopen(xsltin,"r"))) {
    perror(xsltin);
    exit(1);
  }

  memset(&ei,0,sizeof(error_info));

  sroot = xl_snode_new(XSLT_TRANSFORM);
  if (0 != parse_xslt(in,sroot,&ei,xsltin)) {
    error_info_print(&ei,stderr);
    error_info_free_vals(&ei);
    exit(1);
  }

  fclose(in);

  r = process_xslt(sroot);

  if (NULL == (out = fopen(xsltout,"w"))) {
    perror(xsltout);
    exit(1);
  }

  output_xslt(out,sroot);
  fclose(out);

  return r;
}

