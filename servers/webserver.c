#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <libxml/xmlwriter.h>
#include <libxml/xmlIO.h>
#include "util.h"

#define BUFSIZE 1024

typedef struct header {
  char *name;
  char *value;
  struct header *next;
} header;

typedef struct testconn {
  int printed;
  int state;
  int ok;
  int error;
  char *method;
  char *uri;
  char *version;
  header *headers;
  pthread_t thread;
} testconn;

int end_of_line(const char *data, int len, int *pos)
{
  while (*pos < len) {
    if ((*pos < len) && ('\n' == data[*pos]))
      return 1;
    if (((*pos)+1 < len) && ('\r' == data[*pos]) && ('\n' == data[(*pos)+1]))
      return 1;
    (*pos)++;
  }
  return 0;
}

void send_error(int fd, testconn *tc, int code, const char *str, const char *msg)
{
  char *fmt =
    "HTTP/1.1 %d %s\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: %d\r\n"
    "Connection: close\r\n"
    "\r\n"
    "%s\n";
  int len = strlen(fmt)+20+2*strlen(str)+1;
  char *resp = (char*)malloc(len);
  int w;
  snprintf(resp,len,fmt,code,str,strlen(str)+1,str);
  w = write(fd,resp,strlen(resp));
  assert(w == strlen(resp));
  free(resp);
}

#define PS_SRCHNAME     0
#define PS_NAME         1
#define PS_SRCHVALUE    2
#define PS_VALUE        3

void parse_headers(testconn *tc, char *data, int len, int pos)
{
  int namestart = 0;
  int nameend = 0;
  int valstart = 0;
  int valend = 0;
  int state;

  state = PS_NAME;
  namestart = pos;
  while (pos < len) {
    switch (state) {
    case PS_SRCHNAME: {
      if (!isspace(data[pos])) {
        namestart = pos;
        state = PS_NAME;
      }
      else {
        state = PS_VALUE;
      }
    }
    case PS_NAME:
      if (':' == data[pos]) {
        nameend = pos;
        state = PS_SRCHVALUE;
      }
      break;
    case PS_SRCHVALUE:
      if (!isspace(data[pos])) {
        valstart = pos;
        state = PS_VALUE;
      }
      break;
    case PS_VALUE: {
      int nl = 0;
      if ('\n' == data[pos]) {
        valend = pos;
        nl = 1;
      }
      else if ((pos+1 < len) && ('\r' == data[pos]) && ('\n' == data[pos+1])) {
        valend = pos;
        pos++;
        nl = 1;
      }

      if (nl&& ((pos+1 >= len) || !isspace(data[pos+1]))) {
        char *name = (char*)malloc(nameend-namestart+1);
        char *value = (char*)malloc(valend-valstart+1);
        header *h = (header*)calloc(1,sizeof(header));

        while ((valend-1 >= valstart) && isspace(data[valend-1]))
          valend--;

        memcpy(name,&data[namestart],nameend-namestart);
        name[nameend-namestart] = '\0';

        memcpy(value,&data[valstart],valend-valstart);
        value[valend-valstart] = '\0';

        h->name = name;
        h->value = value;
        h->next = tc->headers;
        tc->headers = h;

        namestart = pos+1;
        state = PS_NAME;
      }
      break;
    }
    }
    pos++;
  }
}

void parse_request(int fd, testconn *tc, char *data, int len)
{
  int pos = 0;
  if (!end_of_line(data,len,&pos)) {
    printf("Invalid request line\n");
    send_error(fd,tc,400,"Bad Request","Invalid request line");
    tc->error = 1;
  }

  if (3 != sscanf(data,"%as %as %as",&tc->method,&tc->uri,&tc->version)) {
    printf("Invalid request line\n");
    send_error(fd,tc,400,"Bad Request","Invalid request line");
    tc->error = 1;
  }

  if ((pos < len) && ('\n' == data[pos]))
    pos++;
  else if ((pos+1 < len) && ('\r' == data[pos]) && ('\n' == data[pos+1]))
    pos += 2;

  parse_headers(tc,data,len,pos);
}

void process_request(testconn *tc, int fd)
{
  int fd2 = dup(fd);
  FILE *f = fdopen(fd2,"w");
/*   FILE *src; */
  char *line = NULL;
/*   int n = 0; */

  setbuf(f,NULL);

  fprintf(f,"HTTP/1.1 200 OK\r\n");
  fprintf(f,"Content-Type: text/plain; charset=UTF-8\r\n");
  fprintf(f,"Connection: close\r\n");
  fprintf(f,"\r\n");

/*   if (NULL == (src = fopen(tc->uri,"r"))) { */
/*     fprintf(f,"Can't open file\n"); */
/*   } */
/*   else { */
/*     char buf[BUFSIZE]; */
/*     int r; */
/*     while (0 < (r = fread(buf,1,BUFSIZE,src))) { */
/*       fwrite(buf,1,r,f); */
/*     } */
/*   } */
/*   fclose(f); */

  xmlOutputBufferPtr obuf = xmlOutputBufferCreateFile(f,NULL);
  xmlTextWriterPtr writer = xmlNewTextWriter(obuf);

  xmlTextWriterSetIndent(writer,2);
  xmlTextWriterStartDocument(writer,NULL,NULL,NULL);
  xmlTextWriterStartElement(writer,"doc");
  xmlTextWriterFlush(writer);
/*   while (1 < getline(&line,&n,stdin)) { */
/*     while ((0 < strlen(line)) && ('\n' == line[strlen(line)-1])) */
/*       line[strlen(line)-1] = '\0'; */
/*     printf("line: \"%s\"\n",line); */
/*     xmlTextWriterStartElement(writer,"line"); */
/*     xmlTextWriterWriteFormatAttribute(writer,"length","%d",strlen(line)); */
/*     xmlTextWriterWriteString(writer,line); */
/*     xmlTextWriterEndElement(writer); */
/*     xmlTextWriterFlush(writer); */
/*   } */
  xmlTextWriterEndElement(writer);
  xmlTextWriterEndDocument(writer);

  xmlFreeTextWriter(writer);
  fclose(f);
  free(line);
}

int check_request(char *data, int len, int fd)
{
  int newlines = 0;
  int pos = 0;

  for (pos = 0; (pos < len) && (2 > newlines); pos++) {
    if ((pos < len) && ('\n' == data[pos])) {
      newlines++;
    }
    else if ((pos+1 < len) && ('\r' == data[pos]) && ('\n' == data[pos+1])) {
      pos++;
      newlines++;
    }
    else {
      newlines = 0;
    }
  }

  assert(pos <= len);

  if (2 == newlines) {
    testconn *tc = (testconn*)calloc(1,sizeof(testconn));
    header *h;
    parse_request(fd,tc,data,len);
    if (!tc->error) {
      printf("Got request\n");
      printf("method = %s\n",tc->method);
      printf("uri = %s\n",tc->uri);
      printf("version = %s\n",tc->version);
      for (h = tc->headers; h; h = h->next)
        printf("header %s = %s\n",h->name,h->value);
      process_request(tc,fd);
    }
    return 1;
  }
  return 0;
}

int main(int argc, char **argv)
{
  int listenfd;
  int port;
  struct in_addr addr;

  setbuf(stdout,NULL);

  if (2 > argc) {
    fprintf(stderr,"Usage: webserver <port>\n");
    return 1;
  }

  port = atoi(argv[1]);

  addr.s_addr = INADDR_ANY;
  if (0 > (listenfd = start_listening(addr,port)))
    return 1;

  while (1) {
    struct sockaddr_in remote_addr;
    int sin_size = sizeof(struct sockaddr_in);
    int yes = 1;
    int clientfd;
    array *data = array_new(1);
    int r;
    char buf[BUFSIZE];
    int total = 0;

    if (0 > (clientfd = accept(listenfd,(struct sockaddr*)&remote_addr,&sin_size))) {
      perror("accept");
      exit(1);
    }
    printf("Got connection\n");

    if (0 > setsockopt(clientfd,SOL_TCP,TCP_NODELAY,&yes,sizeof(int))) {
      perror("setsockopt TCP_NODELAY");
      exit(1);
    }

    while (0 < (r = read(clientfd,buf,BUFSIZE))) {
      total += r;
      array_append(data,buf,r);
      if (check_request(data->data,data->nbytes,clientfd))
        break;
    }

    if (0 > r) {
      perror("read");
    }

    printf("connection sent %d total bytes\n",total);

    array_free(data);
    close(clientfd);
  }

  return 0;
}
