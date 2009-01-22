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

public class NumbersServer extends Server {

  public NumbersServer(int maxThreads, int port)
  {
    super(maxThreads,port);
  }

  public void process(InputStream cin, OutputStream cout) throws Exception
  {
    PrintWriter writer = new PrintWriter(cout);
    writer.print(".");
    writer.flush();
    InputStreamReader reader = new InputStreamReader(cin);
    String line = new BufferedReader(reader).readLine();
    String[] tokens = line.split("\\s+");
    int value = Integer.parseInt(tokens[0]);
    int sleep = Integer.parseInt(tokens[1]);
    System.out.println("value = "+value+
                       ", sleep = "+sleep);
    Thread.sleep(sleep);
    writer.print(""+(value*2));
    writer.flush();
  }

//   public static void handle(Socket c, int id)
//     throws Exception {
//     try {
//     }
//     finally {
//       c.close();
//       System.out.format("%10d %10d: connection closed\n",
//                         System.currentTimeMillis(),id);
//     }
//   }

  public static void main(String[] args)
    throws Exception {

    NumbersServer ns = new NumbersServer(1000,1234);
    ns.serve();
  }
}
