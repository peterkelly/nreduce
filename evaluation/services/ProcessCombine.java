package services;

import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.util.Scanner;


import java.io.ByteArrayInputStream;

public class ProcessCombine extends Server
{
  public ProcessCombine(int maxThreads, int port)
  {
    super(maxThreads,port);
  }

  public void process(InputStream cin, OutputStream cout) throws Exception
  {
    PrintWriter writer = new PrintWriter(cout);

//     byte[] readData = readAll(cin);
//     Scanner scanner = new Scanner(new ByteArrayInputStream(readData));
//     System.out.println(new String(readData));

    Scanner scanner = new Scanner(cin);

    if (!scanner.hasNextLine()) // in case of waitconn
      return;

    String line = scanner.nextLine();
    if (line.equals("process")) {
      int index = scanner.nextInt();
      String data = scanner.next();
      writer.print(data);
      writer.flush();
    }
    else if (line.equals("combine")) {
      int len = -1;
      int count = 0;
      String data = null;
      while (scanner.hasNext()) {
        data = scanner.next();
        if (len < 0) {
          len = data.length();
        }
        else if (len != data.length()) {
          System.err.println("Different lengths for combine ("+len+
                             " and "+data.length()+")");
          System.exit(-1);
        }
        count++;
      }

      if (data == null) {
        System.err.println("No values received for combine");
        System.exit(-1);
      }

      writer.print(data);
      writer.flush();
    }
    else {
      System.err.println("Unknown command: "+line);
      System.exit(-1);
    }
  }

  public static void main(String[] args) throws Exception
  {
    if (args.length == 0) {
      System.out.println("Please specify port number");
      System.exit(-1);
    }

    int port = Integer.parseInt(args[0]);
    ProcessCombine comp = new ProcessCombine(3,port);
    comp.serve();
  }
}
