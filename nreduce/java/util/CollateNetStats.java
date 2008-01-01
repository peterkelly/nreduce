package util

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

    void printStats()
    {
        int totalData = 0;
        int totalBetween = 0;
        for (NetStats ns : stats.values()) {
            if (ports.isEmpty() || (ports.contains(ns.fromport) && ports.contains(ns.toport))) {
                System.out.format("%21s %21s %9d %9d\n",
                                  ns.from,ns.to,ns.sent,ns.received);
                totalData += ns.sent;
                totalData += ns.received;
                if (!ns.fromip.equals(ns.toip)) {
                    totalBetween += ns.sent;
                    totalBetween += ns.received;
                }
            }
        }
        System.out.println("Total data transferred: "+totalData+" bytes");
        System.out.println("Total between hosts: "+totalBetween+" bytes");
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

        printStats();
    }

    public static void main(String[] args)
        throws Exception
    {
        Collate c = new Collate();
        c.run(args);
    }
}
