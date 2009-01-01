package services;

import java.net.*;
import java.io.*;

public class Echo extends Server
{
  public Echo(int maxThreads, int port)
  {
    super(maxThreads,port);
  }

  public void process(InputStream cin, OutputStream cout) throws Exception
  {
    byte[] data = readAll(cin);
    cout.write(data);
  }

  public static void main(String[] args) throws Exception
  {
    if (args.length == 0) {
      System.out.println("Please specify port number");
      System.exit(-1);
    }

    int port = Integer.parseInt(args[0]);
    Echo service = new Echo(3,port);
    service.serve();
  }
}
