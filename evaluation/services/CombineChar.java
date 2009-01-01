package services;

import java.net.*;
import java.io.*;

public class CombineChar extends Server
{
  public CombineChar(int maxThreads, int port)
  {
    super(maxThreads,port);
  }

  public void process(InputStream cin, OutputStream cout) throws Exception
  {
    PrintWriter writer = new PrintWriter(cout);
    String input = new String(readAll(cin));
    char[] chars = input.toCharArray();
    int len = chars.length/2;
    char[] reschars = new char[len];

    for (int i = 0; i < len; i++) {
      int letter1 = Math.max(Math.min(chars[i] - 'A',25),0);
      int letter2 = Math.max(Math.min(chars[i*2] - 'A',25),0);
      reschars[i] = (char)('A' + (letter1 + letter2)%26);
    }

    String result = new String(reschars);
    writer.print(result);
    writer.flush();
  }

  public static void main(String[] args) throws Exception
  {
    if (args.length == 0) {
      System.out.println("Please specify port number");
      System.exit(-1);
    }

    int port = Integer.parseInt(args[0]);
    CombineChar service = new CombineChar(3,port);
    service.serve();
  }
}
