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

import java.net.Socket;
import java.net.ServerSocket;
import java.net.InetAddress;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.io.ByteArrayOutputStream;

public abstract class Server
{
  private int maxThreads;
  private int port;
  private int nthreads = 0;

  public Server(int maxThreads, int port)
  {
    this.maxThreads = maxThreads;
    this.port = port;
  }

  public abstract void process(InputStream cin, OutputStream cout) throws Exception;

  public byte[] readAll(InputStream in) throws IOException
  {
    ByteArrayOutputStream bout = new ByteArrayOutputStream();
    byte[] buf = new byte[1024];
    int r;
    while (0 < (r = in.read(buf))) {
      bout.write(buf,0,r);
      // repeat
    }
    return bout.toByteArray();
  }

  public void handle(Socket c, int id)
  {
    System.out.format("%10d %10d: accepted connection\n",
                      System.currentTimeMillis(),id);
    try {
      InputStream cin = c.getInputStream();
      OutputStream cout = c.getOutputStream();

      process(cin,cout);

      /* Make sure we read all remaining data from the client before closing the connection.
         Otherwise if there is outstanding data or a FIN sent by the client, closing the socket
         on our end will result in a RST being sent to the client, aborting the connection and
         potentially causing the client to miss some of the data we've sent back.
      See http://java.sun.com/javase/6/docs/technotes/guides/net/articles/connection_release.html */
      readAll(cin);
    }
    catch (Exception e) {
      e.printStackTrace();
    }
    finally {
      try {
        c.close();
      }
      catch (IOException e) {
        e.printStackTrace();
      }
    }
    System.out.format("%10d %10d: connection closed\n",
                      System.currentTimeMillis(),id);
  }

  private int getNthreads()
  {
    synchronized (this) { 
      return nthreads;
    }
  }

  public void serve()
    throws IOException
  {
    byte[] b = new byte[]{0,0,0,0};
    InetAddress addr = InetAddress.getByAddress(b);
    ServerSocket s = new ServerSocket(port,100,addr);
    System.out.println("Started server socket on port "+port);
    for (int nextid = 0; true; nextid++) {

      synchronized (Server.this) {
        while (nthreads+1 > maxThreads) {
          try {
            System.out.println("nthreads = "+getNthreads()+", waiting");
            Server.this.wait();
          }
          catch (InterruptedException e) {}
        }
        nthreads++;
      }

      final Socket c = s.accept();
      final int id = nextid;

//       System.out.println(id+": connection opened: nthreads = "+getNthreads());

      new Thread() { public void run() {
        try {
          handle(c,id);
          synchronized (Server.this) {
            nthreads--;
            Server.this.notifyAll();
          }
//           System.out.println(id+": connection closed: nthreads = "+getNthreads());
        } catch (Exception e) { }
      } }.start();
    }
  }


}
