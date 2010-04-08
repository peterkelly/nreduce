/*
 * This file is part of the nreduce project
 * Copyright (C) 2009 Peter Kelly (pmk@cs.adelaide.edu.au)
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

import com.sun.net.httpserver.HttpContext;
import com.sun.net.httpserver.HttpServer;
import javax.xml.ws.Endpoint;
import java.net.InetSocketAddress;
import java.util.concurrent.Executors;
import javax.jws.WebService;
import javax.jws.WebMethod;
import javax.jws.soap.SOAPBinding;
import java.io.IOException;
import java.io.BufferedReader;
import java.io.InputStreamReader;

@WebService(targetNamespace="urn:compute")
@SOAPBinding(style=SOAPBinding.Style.RPC,  
             use=SOAPBinding.Use.LITERAL)
public class Compute
{
  int compPerMs = 0;

  public Compute()
  {
    BufferedReader bread = null;
    try {
      String[] args = {"/bin/bash","-c","svc_compute -calibrate"};
      Process proc = Runtime.getRuntime().exec(args);

      bread = new BufferedReader(new InputStreamReader(proc.getInputStream()));
      try {
        String line;
        String search = "comp per ms = ";
        while ((line = bread.readLine()) != null) {
          System.out.println(line);
          if (line.indexOf(search) >= 0)
            compPerMs = Integer.parseInt(line.substring(search.length()));
        }
      }
      finally {
        try { bread.close(); }
        catch (IOException e) {}
      }

      int rc = proc.waitFor();
      if (rc != 0) {
        System.err.println("svc_compute exited with status "+rc);
        compPerMs = 0;
      }
    }
    catch (IOException e) {
      e.printStackTrace();
    }
    catch (InterruptedException e) {
      e.printStackTrace();
    }
    finally {
      if (compPerMs == 0) {
        System.err.println("Calibration failed");
        System.exit(1);
      }
    }
  }

  @WebMethod
  public int compute(int value, int ms)
  {
    System.out.println("Starting compute "+value+" "+ms);
    String[] args = {"/bin/bash","-c","compute "+ms};
    BufferedReader bread = null;
    try {
      String[] envp = {"PATH="+System.getenv("PATH"),
                       "COMP_PER_MS="+compPerMs};
      Process proc = Runtime.getRuntime().exec(args,envp);

      bread = new BufferedReader(new InputStreamReader(proc.getInputStream()));
      String line;
      while ((line = bread.readLine()) != null)
        System.out.println(line);

      bread = new BufferedReader(new InputStreamReader(proc.getErrorStream()));
      try {
        while ((line = bread.readLine()) != null)
          System.out.println(line);
      }
      finally {
        try { bread.close(); }
        catch (IOException e) {}
      }

      int rc = proc.waitFor();


      if (rc != 0) {
        System.err.println("compute exited with status "+rc);
        return -1;
      }


    } catch (IOException e) {
      e.printStackTrace();
      return -1;
    }
    catch (InterruptedException e) {
      e.printStackTrace();
      return -1;
    }

    return value;
  }

  public static void main(String[] args)
    throws IOException
  {
    if (args.length < 1) {
      System.err.println("Please specify port number");
      System.exit(1);
    }
    int port = Integer.parseInt(args[0]);

    Compute compute = new Compute();

    System.setProperty("sun.net.httpserver.readTimeout","3600");

    HttpServer server = HttpServer.create(new InetSocketAddress(port), 5);
    server.setExecutor(Executors.newFixedThreadPool(3));

    Endpoint endpoint = Endpoint.create(compute);
    endpoint.publish(server.createContext("/compute"));

    HttpContext context = server.createContext("/");
    context.setHandler(new BasicHandler());

    server.start();
    System.out.println("Compute service started");
  }
}
