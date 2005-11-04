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

#ifndef _HTTP_HTTP_H
#define _HTTP_HTTP_H

#define HTTP_VERSION "1.1"

#define HTTP_CONTINUE                        100
#define HTTP_SWITCHING_PROTOCOLS             101
#define HTTP_OK                              200
#define HTTP_CREATED                         201
#define HTTP_ACCEPTED                        202
#define HTTP_NON_AUTHORITATIVE_INFORMATION   203
#define HTTP_NO_CONTENT                      204
#define HTTP_RESET_CONTENT                   205
#define HTTP_PARTIAL_CONTENT                 206
#define HTTP_MULTIPLE_CHOICES                300
#define HTTP_MOVED_PERMANENTLY               301
#define HTTP_FOUND                           302
#define HTTP_SEE_OTHER                       303
#define HTTP_NOT_MODIFIED                    304
#define HTTP_USE_PROXY                       305
#define HTTP_UNUSED                          306
#define HTTP_TEMPORARY_REDIRECT              307
#define HTTP_BAD_REQUEST                     400
#define HTTP_UNAUTHORIZED                    401
#define HTTP_PAYMENT_REQUIRED                402
#define HTTP_FORBIDDEN                       403
#define HTTP_NOT_FOUND                       404
#define HTTP_METHOD_NOT_ALLOWED              405
#define HTTP_NOT_ACCEPTABLE                  406
#define HTTP_PROXY_AUTHENTICATION_REQUIRED   407
#define HTTP_REQUEST_TIMEOUT                 408
#define HTTP_CONFLICT                        409
#define HTTP_GONE                            410
#define HTTP_LENGTH_REQUIRED                 411
#define HTTP_PRECONDITION_FAILED             412
#define HTTP_REQUEST_ENTITY_TOO_LARGE        413
#define HTTP_REQUEST_URI_TOO_LONG            414
#define HTTP_UNSUPPORTED_MEDIA_TYPE          415
#define HTTP_REQUESTED_RANGE_NOT_SATISFIABLE 416
#define HTTP_EXPECTATION_FAILED              417
#define HTTP_INTERNAL_SERVER_ERROR           500
#define HTTP_NOT_IMPLEMENTED                 501
#define HTTP_BAD_GATEWAY                     502
#define HTTP_SERVICE_UNAVAILABLE             503
#define HTTP_GATEWAY_TIMEOUT                 504
#define HTTP_HTTP_VERSION_NOT_SUPPORTED      505

#define HTTP_VERSION_1_1 0
#define HTTP_VERSION_1_0 1

#define PORT 8080
#define SERVER_URL "http://localhost:8080"

typedef struct rctext rctext;
typedef struct server server;

struct rctext {
  int rc;
  char *text;
};

struct server {
  char *docroot;
};

#ifndef _HTTP_HTTP_C
extern struct rctext response_codes[41];
#endif

int http_main(const char *docroot);

#endif /* _HTTP_HTTP_H */
