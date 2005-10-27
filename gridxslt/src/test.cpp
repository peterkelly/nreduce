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

#include "util/String.h"
#include "util/list.h"
#include "util/xmlutils.h"
#include "xmlschema/xmlschema.h"
#include "dataflow/SequenceType.h"
#include "dataflow/Program.h"
#include <stdio.h>

using namespace GridXSLT;

int main(int argc, char **argv)
{
#if 0
  xs_init();

  setbuf(stdout,NULL);

  String a = "foo";
  String b = "bar";
  String c = "foo";
  String both = a + b;
  String big = "The quick brown fox jumps over the lazy dog";
  both.dump();
  message("\n");
  message("a == b ? %d\n",a == b);
  message("a == c ? %d\n",a == c);
  message("a == \"bar\" ? %d\n",a == "bar");
  message("a == \"foo\" ? %d\n",a == "foo");
  message("a != \"bar\" ? %d\n",a != "bar");
  message("a != \"foo\" ? %d\n",a != "foo");
  message("\"bar\" == a ? %d\n","bar" == a);
  message("\"foo\" == a ? %d\n","foo" == a);
  big.substring(6).dump();
  message("\n");
  big.substring(600).dump();
  message("\n");
  big.substring(6,5).dump();
  message("\n");
  String temp = big.substring(6,50);
  message("%*\n",&temp);
  message("indexof = %d\n",big.indexOf("fox"));

  List<int> ints;
  message("&ints = %p\n",&ints);
  ints.append(1);
  ints.append(2);
  ints.append(3);
  ints.append(4);
  ints.append(5);
//   message("%p\n",ints.first);
//   message("%p\n",ints.first->next);
//   message("%p\n",ints.first->next->next);
  Iterator<int> it;
  for (it = ints; it.haveCurrent(); it++)
    message("%d\n",*it);

  message("count = %d\n",ints.count());
  message("------\n");

  List<String> strings;
  strings.push("this");
  strings.push("is");
  strings.push("a");
  strings.push("test");
  Iterator<String> it2;
  for (it2 = strings; it2.haveCurrent(); it2++)
    it2->dump();

  message("count = %d\n",strings.count());
  message("------\n");

  strings.pop();
//   it2 = strings;
//   it2++;
//   it2++;
//   it2++;
//   it2.remove();

  for (it2 = strings; it2.haveCurrent(); it2++)
    it2->dump();
  message("count = %d\n",strings.count());

  strings.pop();
  message("isEmpty() ? %d\n",strings.isEmpty());
  strings.pop();
  message("isEmpty() ? %d\n",strings.isEmpty());
  strings.pop();
  message("isEmpty() ? %d\n",strings.isEmpty());

  List<int> ints2;
  ints2 = ints;
  ints.pop();

  for (it = ints2; it.haveCurrent(); it++)
    message("%d\n",*it);
  message("ints.count() = %d\n",ints.count());
  message("ints2.count() = %d\n",ints2.count());

  StringBuffer buf;
  buf.format("test %d: %-10d|%4d|%s",4,5,6,"foo");
  buf.contents().dump();
  message("\n");

  SequenceType left = SequenceType::item();
  SequenceType right = SequenceType(xs_g->int_type);
  SequenceType st = SequenceType::choice(left,right);
  buf.format("Type is: %*!",&st);
  buf.contents().dump();
  message("\n");

  buf.clear();


  xs_cleanup();
#endif

#if 0
  String a = "foo";
  String b = "bar";
  message("%*:%*\n",&a,&b);
#endif

  QName q1 = QName("foo","bar");
  QName q2 = QName("foo","bar2");
  QName q3 = QName("foo","bar");
  message("q1 == q2 ? %d\n",q1 == q2);
  message("q1 == q3 ? %d\n",q1 == q3);

  return 0;
}
