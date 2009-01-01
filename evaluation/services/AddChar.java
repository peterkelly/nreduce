package services;

import java.net.*;
import java.io.*;

public class AddChar extends Server
{
  public AddChar(int maxThreads, int port)
  {
    super(maxThreads,port);
  }

  public void process(InputStream cin, OutputStream cout) throws Exception
  {
    PrintWriter writer = new PrintWriter(cout);
    String input = new String(readAll(cin));
    char[] chars = input.toCharArray();
    for (int i = 0; i < chars.length; i++) {
      int letter = Math.max(Math.min(chars[i] - 'A',25),0);
      chars[i] = (char)('A' + (letter+1)%26);
    }
    String result = new String(chars);
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
    AddChar service = new AddChar(3,port);
    service.serve();
  }
}
