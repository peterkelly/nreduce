package util;

import java.util.*;
import java.util.regex.*;
import java.io.*;
import java.net.*;

public class CollateNetStats
{
  Pattern firstPattern =
    Pattern.compile(".*listening addr = (\\d+\\.\\d+\\.\\d+\\.\\d+\\:\\d+)$");

  Pattern linePattern =
    Pattern.compile(".*NETSTATS\\s+(\\d+\\.\\d+\\.\\d+\\.\\d+\\:\\d+)"+
                    "\\s+(\\d+)\\s+(\\d+)\\s*$");

  Set<Integer> ports = new HashSet<Integer>();
  Map<String,NetStats> stats = new TreeMap<String,NetStats>();

  class NetStats
  {
    String from;
    String to;
    InetAddress fromip;
    InetAddress toip;
    int fromport;
    int toport;
    int sent;
    int received;

    public NetStats(String from, String to, int sent, int received)
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

  String sizeStr(int bytes)
  {
    if (bytes >= 1024*1024)
      return String.format("%.3f MB",((double)bytes)/(1024.0*1024.0));
    if (bytes >= 1024)
      return String.format("%.3f KB",((double)bytes)/1024.0);
    return bytes+" bytes";
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
    int totalData = 0;
    for (NetStats ns : stats.values()) {
      if (ports.isEmpty() || (ports.contains(ns.fromport) && ports.contains(ns.toport))) {
        if (!betweenonly || (!ns.fromip.equals(ns.toip))) {
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

  void addStats(String from, String to, int sent, int received)
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
    String first = br.readLine();
    if (null == first)
      throw new Exception("No first line in "+f.getName());
    Matcher m = firstPattern.matcher(first);
    if (!m.matches())
      throw new Exception("Invalid first line in "+f.getName());
    String thisip = m.group(1);

    String line;
    while (null != (line = br.readLine())) {
      m = linePattern.matcher(line);
      if (m.matches()) {
        String otherip = m.group(1);
        int sent = Integer.parseInt(m.group(2));
        int received = Integer.parseInt(m.group(3));

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
