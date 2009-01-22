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

import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.util.Scanner;


import java.io.ByteArrayInputStream;

public class ProcessCombine extends Server
{
  public ProcessCombine(int maxThreads, int port)
  {
    super(maxThreads,port);
  }

  public void process(InputStream cin, OutputStream cout) throws Exception
  {
    PrintWriter writer = new PrintWriter(cout);

//     byte[] readData = readAll(cin);
//     Scanner scanner = new Scanner(new ByteArrayInputStream(readData));
//     System.out.println(new String(readData));

    Scanner scanner = new Scanner(cin);

    if (!scanner.hasNextLine()) // in case of waitconn
      return;

    String line = scanner.nextLine();
    if (line.equals("process")) {
      int index = scanner.nextInt();
      String data = scanner.next();
      writer.print(data);
      writer.flush();
    }
    else if (line.equals("combine")) {
      int len = -1;
      int count = 0;
      String data = null;
      while (scanner.hasNext()) {
        data = scanner.next();
        if (len < 0) {
          len = data.length();
        }
        else if (len != data.length()) {
          System.err.println("Different lengths for combine ("+len+
                             " and "+data.length()+")");
          System.exit(-1);
        }
        count++;
      }

      if (data == null) {
        System.err.println("No values received for combine");
        System.exit(-1);
      }

      writer.print(data);
      writer.flush();
    }
    else {
      System.err.println("Unknown command: "+line);
      System.exit(-1);
    }
  }

  public static void main(String[] args) throws Exception
  {
    if (args.length == 0) {
      System.out.println("Please specify port number");
      System.exit(-1);
    }

    int port = Integer.parseInt(args[0]);
    ProcessCombine comp = new ProcessCombine(3,port);
    comp.serve();
  }
}
