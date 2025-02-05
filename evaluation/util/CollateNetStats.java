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

package util;

import java.util.*;
import java.util.regex.*;
import java.io.*;
import java.net.*;

public class CollateNetStats
{
  Pattern ipPattern =
    Pattern.compile(".*listening addr = (\\d+\\.\\d+\\.\\d+\\.\\d+\\:\\d+)$");

  Pattern linePattern =
    Pattern.compile(".*NETSTATS\\s+(\\d+\\.\\d+\\.\\d+\\.\\d+\\:\\d+)"+
                    "\\s+(\\d+)\\s+(\\d+)\\s*$");

  Set<Integer> ports = new HashSet<Integer>();
  Map<String,NetStats> stats = new TreeMap<String,NetStats>();
  InetAddress localhost;

  class NetStats
  {
    String from;
    String to;
    InetAddress fromip;
    InetAddress toip;
    int fromport;
    int toport;
    long sent;
    long received;

    public NetStats(String from, String to, long sent, long received)
      throws Exception
    {
      this.from = from;
      this.to = to;
      this.fromip = getIP(from);
      this.toip = getIP(to);
      this.fromport = getPort(from);
      this.toport = getPort(to);
      this.sent = sent;
      this.received = received;
    }
  }

  String sizeStr(long bytes)
  {
    return String.format("%.3f KB",((double)bytes)/1024.0);
  }

  int getPort(String str)
  {
    Pattern p = Pattern.compile("\\d+\\.\\d+\\.\\d+\\.\\d+\\:(\\d+)");
    Matcher m = p.matcher(str);
    if (m.matches())
      return Integer.parseInt(m.group(1));
    else
      return 0;
  }

  InetAddress getIP(String str)
    throws Exception
  {
    Pattern p = Pattern.compile("(\\d+\\.\\d+\\.\\d+\\.\\d+)\\:\\d+");
    Matcher m = p.matcher(str);
    if (m.matches())
      return InetAddress.getByName(m.group(1));
    else
      throw new Exception("No such match");
  }

  void printStats(String type, boolean betweenonly)
  {
    long totalData = 0;
    for (NetStats ns : stats.values()) {
      if (ports.isEmpty() || (ports.contains(ns.fromport) && ports.contains(ns.toport))) {

        boolean count = true;
        if (betweenonly) {
          count = !ns.fromip.equals(ns.toip) &&
            !ns.fromip.equals(localhost) &&
            !ns.toip.equals(localhost);
        }

        if (count) {
          System.out.format("%21s %21s %9d %9d\n",
                            ns.from,ns.to,ns.sent,ns.received);
          totalData += ns.sent;
          totalData += ns.received;
        }
      }
    }
    System.out.println("Total data "+type+": "+sizeStr(totalData));
  }

  public void printDot(String filename)
    throws IOException
  {
    PrintWriter out = new PrintWriter(filename);
    out.println("digraph {");
    out.println("  rotate=90;");

    int nextid = 0;
    Set<String> ips = new HashSet<String>(); // set of all machiens
    Set<String> ipports = new HashSet<String>();
    Map<String,String> nodes = new HashMap<String,String>();
    for (NetStats ns : stats.values()) {
      ips.add(ns.fromip.getHostAddress());
      ips.add(ns.toip.getHostAddress());
      ipports.add(ns.from);
      ipports.add(ns.to);
      if (!nodes.containsKey(ns.from))
        nodes.put(ns.from,"n"+(nextid++));
      if (!nodes.containsKey(ns.to))
        nodes.put(ns.to,"n"+(nextid++));
    }

    int clusterno = 0;
    for (String ip : ips) {
      out.println("  subgraph cluster"+(clusterno++)+" {");
      out.println("    label=\""+ip+"\"");
      for (String ipport : ipports) {
        if (ipport.startsWith(ip+":")) {
          int colon = ipport.indexOf(":");
          String portstr = ipport.substring(colon+1);
          out.println("    "+nodes.get(ipport)+" [label=\""+portstr+"\"];");
        }
      }
      out.println("  }");
    }

    for (NetStats ns : stats.values()) {
      if (ns.sent > 102400)
        out.println("  "+nodes.get(ns.from)+" -> "+nodes.get(ns.to)+" [label=\""+ns.sent+"\"];");
    }

    out.println("}");
    out.close();
  }

  void addStats(String from, String to, long sent, long received)
    throws Exception
  {
    String key = from+" "+to;
    if (stats.containsKey(key)) {
      NetStats existing = stats.get(key);
      if ((existing.sent != sent) || (existing.received != received))
        throw new Exception("Differing results for "+key);
    }
    else {
      NetStats ns = new NetStats(from,to,sent,received);
      stats.put(key,ns);
    }
  }

  void processFile(File f, Map<String,NetStats> stats)
    throws Exception
  {
    BufferedReader br = new BufferedReader(new FileReader(f));

    String thisip = null;
    Matcher m;
    do {
      String line = br.readLine();
      if (line == null)
        throw new Exception("Cannot find line specifying IP address in "+f.getName());
      m = ipPattern.matcher(line);
    } while (!m.matches());

    thisip = m.group(1);

    String line;
    while (null != (line = br.readLine())) {
      m = linePattern.matcher(line);
      if (m.matches()) {
        String otherip = m.group(1);
        long sent = Long.parseLong(m.group(2));
        long received = Long.parseLong(m.group(3));

        if (thisip.compareTo(otherip) < 0)
          addStats(thisip,otherip,sent,received);
        else
          addStats(otherip,thisip,received,sent);
      }
    }
  }

  public void run(String[] args)
    throws Exception
  {
    if (args.length == 0) {
      System.err.println("Please specify log dir");
      System.exit(-1);
    }

    localhost = InetAddress.getByName("127.0.0.1");

    String logdir = args[0];
    for (int i = 0; i < args.length-1; i++)
      ports.add(Integer.parseInt(args[i+1]));

    File dir = new File(logdir);
    for (File f : dir.listFiles()) {
      if (f.getName().matches(".*\\.log$"))
        processFile(f,stats);
    }

    System.out.println("Between processes");
    printStats("between processes",false);
    System.out.println();
    System.out.println("Between machines");
    printStats("between machines",true);
    System.out.println();
//     printDot("transfer.dot");
  }

  public static void main(String[] args)
    throws Exception
  {
    CollateNetStats cns = new CollateNetStats();
    cns.run(args);
  }
}
