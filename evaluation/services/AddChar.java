package services;

import java.net.*;
import java.io.*;

public class AddChar
{
  public static void handle(Socket c, int id) throws Exception
  {
    try {
      InputStream cin = c.getInputStream();
      OutputStream cout = c.getOutputStream();
      PrintWriter writer = new PrintWriter(cout);
      InputStreamReader reader = new InputStreamReader(cin);

      String line = new BufferedReader(reader).readLine();
      System.out.println("line = \""+line+"\"");
      char[] chars = line.toCharArray();
      for (int i = 0; i < chars.length; i++)
        chars[i]++;
      String result = new String(chars);
      writer.print(result);
      writer.flush();
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
