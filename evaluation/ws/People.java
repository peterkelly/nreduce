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
 * $Id: Empty.java 851 2009-01-22 01:19:14Z pmkelly $
 *
 */

import com.sun.net.httpserver.HttpContext;
import com.sun.net.httpserver.HttpServer;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.Executors;
import javax.jws.WebMethod;
import javax.jws.WebService;
import javax.jws.soap.SOAPBinding;
import javax.xml.ws.Endpoint;

@WebService(targetNamespace="urn:people")
@SOAPBinding(style=SOAPBinding.Style.RPC,  
             use=SOAPBinding.Use.LITERAL) 
public class People
{
  static Person[] people = {
      new Person("Fred",20,"Plumber"),
      new Person("Joe",33,"Marketing executive"),
      new Person("Bob",38,"Builder")
    };

/*  static {
    people = new Person[10000];
    for (int i = 0; i < people.length; i++)
      people[i] = new Person("Person"+i,i,"Occupation"+i);
  }*/

  @WebMethod
    public String[] getNames()
  {
    String[] names = new String[people.length];
    for (int i = 0; i < people.length; i++)
      names[i] = people[i].name;
    return names;
  }

  @WebMethod
    public Person getPerson(String name)
  {
    for (int i = 0; i < people.length; i++)
      if (people[i].name.equals(name))
        return people[i];
    return null;
  }

  @WebMethod
    public Person[] getAllPeople()
  {
    return people;
  }

  @WebMethod
    public int totalAges(Person[] p)
  {
    int total = 0;
    for (int i = 0; i < p.length; i++)
      total += p[i].age;
    return total;
  }

  @WebMethod
    public String describe(Person p)
  {
    return p.name+" ("+p.age+"), "+p.occupation;
  }

  @WebMethod
    public String test()
  {
    return "Hello";
  }

  @WebMethod
    public int add(int a, int b)
  {
    return a + b;
  }

  public static void main(String[] args)
    throws IOException
  {
    if (args.length < 1) {
      System.err.println("Please specify port number");
      System.exit(1);
    }
    int port = Integer.parseInt(args[0]);

    System.setProperty("sun.net.httpserver.readTimeout","3600");

    HttpServer server = HttpServer.create(new InetSocketAddress(port), 5);
    server.setExecutor(Executors.newFixedThreadPool(3));
    server.start();

    Endpoint endpoint = Endpoint.create(new People());
    endpoint.publish(server.createContext("/people"));

    HttpContext context = server.createContext("/");
    context.setHandler(new BasicHandler());

    System.out.println("People service started");
  }
}
