package services;

import java.net.*;
import java.io.*;
import java.util.*;

public class Compute
{
  static int nfib(int n)
  {
    if (n <= 1)
      return 1;
    else
      return nfib(n-2)+nfib(n-1);
  }

  static void compute(int delay)
  {
    for (int i = 0; i < delay; i++)
      nfib(32);
  }

  public static void handle(Socket c, int id) throws Exception
  {
    try {
      InputStream cin = c.getInputStream();
      OutputStream cout = c.getOutputStream();
      PrintWriter writer = new PrintWriter(cout);
      InputStreamReader reader = new InputStreamReader(cin);

      String line = new BufferedReader(reader).readLine();
      System.out.println(line);
      String[] tokens = line.split("\\s+");
      int value = Integer.parseInt(tokens[0]);
      int delay = Integer.parseInt(tokens[1]);
      long start = System.nanoTime();
      compute(delay);
      long end = System.nanoTime();
      int ms = (int)((end-start)/1000000);
      writer.println("done: "+ms+" ("+value+")");
      writer.flush();
    }
    catch (Exception e) {
      System.out.println(e);
    }
    finally {
      c.close();
      System.out.println(id+": connection closed");
    }
  }

  public static void main(String[] args) throws Exception
  {
    if (args.length == 0) {
      System.out.println("Please specify port number");
      System.exit(-1);
    }

    int port = Integer.parseInt(args[0]);

    byte[] b = new byte[]{0,0,0,0};
    InetAddress addr = InetAddress.getByAddress(b);
    ServerSocket s = new ServerSocket(port,100,addr);
    System.out.println("Started server socket on port "+port);
    for (int nextid = 0; true; nextid++) {
      final Socket c = s.accept();
      final int id = nextid;
      System.out.println(id+": accepted connection");
      new Thread() { public void run() {
        try { handle(c,id); } catch (Exception e) { }
      } }.start();
    }
  }
}
