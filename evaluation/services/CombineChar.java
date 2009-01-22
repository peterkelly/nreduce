/*
 * This file is part of the nreduce project
 * Copyright (C) 2008-2009 Peter Kelly (pmk@cs.adelaide.edu.au)
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

package services;

import java.net.*;
import java.io.*;

public class CombineChar extends Server
{
  public CombineChar(int maxThreads, int port)
  {
    super(maxThreads,port);
  }

  public void process(InputStream cin, OutputStream cout) throws Exception
  {
    PrintWriter writer = new PrintWriter(cout);
    String input = new String(readAll(cin));
    char[] chars = input.toCharArray();
    int len = chars.length/2;
    char[] reschars = new char[len];

    for (int i = 0; i < len; i++) {
      int letter1 = Math.max(Math.min(chars[i] - 'A',25),0);
      int letter2 = Math.max(Math.min(chars[i*2] - 'A',25),0);
      reschars[i] = (char)('A' + (letter1 + letter2)%26);
    }

    String result = new String(reschars);
    writer.print(result);
    writer.flush();
  }

  public static void main(String[] args) throws Exception
  {
    if (args.length == 0) {
      System.out.println("Please specify port number");
      System.exit(-1);
    }

    int port = Integer.parseInt(args[0]);
    CombineChar service = new CombineChar(3,port);
    service.serve();
  }
}
