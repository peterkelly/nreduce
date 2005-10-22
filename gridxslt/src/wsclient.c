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

#include "util/list.h"
#include "util/stringbuf.h"
#include "syntax_wsdl.h"
#include <stdio.h>
#include <libxml/xmlwriter.h>
#include <libxml/xmlIO.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

size_t write_curl(void *ptr, size_t size, size_t nmemb, void *data)
{
  printf("WRITE_CURL %d\n",size*nmemb);
  stringbuf_append((stringbuf*)data,(const char*)ptr,size*nmemb);
  return size*nmemb;
}


int	writefn(void *context, const char *buffer, int len)
{
/*   printf("WRITE %d\n",len); */
  stringbuf_append((stringbuf*)context,buffer,len);
  return len;
}

int	closefn(void *context)
{
/*   printf("CLOSE\n"); */
  return 0;
}

void test()
{
  stringbuf *sbuf = stringbuf_new();
  stringbuf *response = stringbuf_new();
  xmlOutputBuffer *buf = xmlOutputBufferCreateIO(writefn,closefn,sbuf,NULL);
  xmlTextWriter *writer = xmlNewTextWriter(buf);
  char *url = "http://localhost.localdomain:8080/test-jaxrpc/test";
  struct curl_slist *headers = NULL;
  CURL *h;
  CURLcode cr;

  xmlTextWriterSetIndent(writer,1);
  xmlTextWriterSetIndentString(writer,"  ");
  xmlTextWriterStartDocument(writer,NULL,NULL,NULL);
  xmlTextWriterStartElement(writer,"env:Envelope");

  xmlTextWriterWriteAttribute(writer,"xmlns:env","http://schemas.xmlsoap.org/soap/envelope/");
  xmlTextWriterWriteAttribute(writer,"xmlns:xsd","http://www.w3.org/2001/XMLSchema");
  xmlTextWriterWriteAttribute(writer,"xmlns:xsi","http://www.w3.org/2001/XMLSchema-instance");
  xmlTextWriterWriteAttribute(writer,"xmlns:enc","http://schemas.xmlsoap.org/soap/encoding/");
  xmlTextWriterWriteAttribute(writer,"xmlns:ns0","urn:Foo");
  xmlTextWriterWriteAttribute(writer,"env:encodingStyle","http://schemas.xmlsoap.org/soap/encoding/");

  xmlTextWriterStartElement(writer,"env:Body");




/*   xmlTextWriterStartElement(writer,"ns0:sayHello2"); */

/*   xmlTextWriterStartElement(writer,"String_1"); */
/*   xmlTextWriterWriteAttribute(writer,"xsi:type","xsd:string"); */
/*   xmlTextWriterWriteRaw(writer,"Fred"); */
/*   xmlTextWriterEndElement(writer); */

/*   xmlTextWriterStartElement(writer,"String_2"); */
/*   xmlTextWriterWriteAttribute(writer,"xsi:type","xsd:string"); */
/*   xmlTextWriterWriteRaw(writer,"Peter"); */
/*   xmlTextWriterEndElement(writer); */

/*   xmlTextWriterEndElement(writer); */


/*   xmlTextWriterStartElement(writer,"ns0:listFiles"); */

/*   xmlTextWriterStartElement(writer,"String_1"); */
/*   xmlTextWriterWriteAttribute(writer,"xsi:type","xsd:string"); */
/*   xmlTextWriterWriteRaw(writer,"/etc"); */
/*   xmlTextWriterEndElement(writer); */

/*   xmlTextWriterEndElement(writer); */




  xmlTextWriterStartElement(writer,"ns0:getMatrix");
  xmlTextWriterEndElement(writer);








  xmlTextWriterEndElement(writer);
  xmlTextWriterEndElement(writer);
  xmlTextWriterEndDocument(writer);
  xmlTextWriterFlush(writer);
  xmlFreeTextWriter(writer);

  printf("XML file:\n");
  printf("===============\n");
  printf("%s",sbuf->data);
  printf("===============\n");

#ifndef DISABLE_CURL
  curl_global_init(CURL_GLOBAL_ALL);
  h = curl_easy_init();

  headers = curl_slist_append(headers,"Content-Type: text/xml; charset=utf-8");
/*   headers = curl_slist_append(headers,"Content-Length: 454"); */
  headers = curl_slist_append(headers,"SOAPAction: \"\"");
  headers = curl_slist_append(headers,"Accept: text/html, image/gif, image/jpeg, *; q=.2, */*; q=.2");
  curl_easy_setopt(h,CURLOPT_URL,url);
  curl_easy_setopt(h,CURLOPT_POST,1);
  curl_easy_setopt(h,CURLOPT_POSTFIELDS,sbuf->data);
  curl_easy_setopt(h,CURLOPT_HTTPHEADER,headers);
  curl_easy_setopt(h,CURLOPT_WRITEFUNCTION,write_curl);
  curl_easy_setopt(h,CURLOPT_WRITEDATA,response);
  curl_easy_setopt(h,CURLOPT_USERAGENT,"libcurl-agent/1.0");
  cr = curl_easy_perform(h);
  curl_easy_cleanup(h);
#endif

  printf("Response:\n");
  printf("===============\n");
  printf("%s\n",response->data);
  printf("===============\n");


  stringbuf_free(sbuf);
  stringbuf_free(response);
}

int main(int argc, char **argv)
{
  char *wsdl_filename;
  FILE *f;
  wsdl *w;
  int r = 0;

  test();
  exit(0);

  if (2 > argc) {
    fprintf(stderr,"Usage: wsclient <wsdl_file>\n");
    exit(1);
  }

  wsdl_filename = argv[1];

  if (NULL == (f = fopen(wsdl_filename,"r"))) {
    perror(wsdl_filename);
    exit(1);
  }

  w = parse_wsdl(f);
  if (NULL == w) {
    r = 1;
  }
  else {
    ws_message *m;
    ws_port_type *pt;
    ws_binding *b;
    ws_service *s;

    printf("Parsed WSDL file:\n");

    for (m = w->messages; m; m = m->next) {
      ws_part *p;
      printf("  message \"%s\"\n",m->name);
      for (p = m->parts; p; p = p->next) {
        if (p->element)
          printf("    part \"%s\" (element %s)\n",p->name,p->element);
        else
          printf("    part \"%s\" (type %s)\n",p->name,p->type);
      }
    }

    for (pt = w->port_types; pt; pt = pt->next) {
      ws_operation *o;
      ws_param *p;
      printf("  port type \"%s\"\n",pt->name);
      for (o = pt->operations; o; o = o->next) {
        printf("    operation \"%s\"\n",o->name);
        if (o->input)
          printf("      input message \"%s\"\n",o->input->message);
        if (o->output)
          printf("      output message \"%s\"\n",o->output->message);
        for (p = o->faults; p; p = p->next)
          printf("      fault message \"%s\"\n",p->message);
      }
    }

    for (b = w->bindings; b; b = b->next) {
      ws_bindop *bo;
      printf("  binding \"%s\" for port type \"%s\"\n",b->name,b->type);
      for (bo = b->operations; bo; bo = bo->next) {
        ws_bindparam *bp;
        printf("    bindop \"%s\"\n",bo->name);
        if (bo->input)
          printf("      input\n");
        if (bo->output)
          printf("      output\n");
        for (bp = bo->faults; bp; bp = bp->next)
          printf("      fault \"%s\"\n",bp->name);
      }
    }

    for (s = w->services; s; s = s->next) {
      ws_port *p;
      printf("  service \"%s\"\n",s->name);
      for (p = s->ports; p; p = p->next) {
        printf("    port \"%s\" binding \"%s\"\n",p->name,p->binding);
      }
    }



  }

  fclose(f);






/*   test(); */
  return r;
}
