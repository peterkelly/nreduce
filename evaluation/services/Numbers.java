package services;

import java.net.*;
import java.io.*;

public class Numbers {

  public static void handle(Socket c, int id)
    throws Exception {
    try {
      InputStream cin = c.getInputStream();
      OutputStream cout = c.getOutputStream();
      PrintWriter writer = new PrintWriter(cout);
      InputStreamReader reader = new InputStreamReader(cin);
      String line = new BufferedReader(reader).readLine();
      String[] tokens = line.split("\\s+");
      int value = Integer.parseInt(tokens[0]);
      int sleep = Integer.parseInt(tokens[1]);
      System.out.println(id+": value = "+value+
                         ", sleep = "+sleep);
      Thread.sleep(sleep);
      writer.print(""+(value*2));
      writer.flush();
    }
    finally {
      c.close();
      System.out.println(id+": connection closed");
    }
  }

  public static void main(String[] args)
    throws Exception {
    byte[] b = new byte[]{0,0,0,0};
    InetAddress addr = InetAddress.getByAddress(b);
    ServerSocket s = new ServerSocket(1234,100,addr);
    System.out.println("Started server socket");
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
