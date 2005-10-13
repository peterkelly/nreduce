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

#ifndef _HTTP_HANDLERS_H
#define _HTTP_HANDLERS_H

#include "http.h"
#include "message.h"

typedef struct urihandler urihandler;

struct urihandler {
  int (*read)(urihandler *uh, const char *buf, int size);
  int (*write)(urihandler *uh, msgwriter *mw);
  void (*cleanup)(urihandler *uh);
  void *d;
};

urihandler *handle_file(const char *uri, msgwriter *mw, int size);
urihandler *handle_dir(const char *uri, msgwriter *mw);
urihandler *handle_tree(const char *uri, msgwriter *mw);

#endif /* _HTTP_HANDLERS_H */
