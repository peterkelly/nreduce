package services;

import java.net.*;
import java.io.*;

public class NumbersServer extends Server {

  public NumbersServer(int maxThreads, int port)
  {
    super(maxThreads,port);
  }

  public void process(InputStream cin, OutputStream cout) throws Exception
  {
    PrintWriter writer = new PrintWriter(cout);
    writer.print(".");
    writer.flush();
    InputStreamReader reader = new InputStreamReader(cin);
    String line = new BufferedReader(reader).readLine();
    String[] tokens = line.split("\\s+");
    int value = Integer.parseInt(tokens[0]);
    int sleep = Integer.parseInt(tokens[1]);
    System.out.println("value = "+value+
                       ", sleep = "+sleep);
    Thread.sleep(sleep);
    writer.print(""+(value*2));
    writer.flush();
  }

//   public static void handle(Socket c, int id)
//     throws Exception {
//     try {
//     }
//     finally {
//       c.close();
//       System.out.format("%10d %10d: connection closed\n",
//                         System.currentTimeMillis(),id);
//     }
//   }

  public static void main(String[] args)
    throws Exception {

    NumbersServer ns = new NumbersServer(1000,1234);
    ns.serve();
  }
}
