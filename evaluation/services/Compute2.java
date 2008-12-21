package services;

import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.util.Scanner;

public class Compute2 extends Server
{
  public Compute2(int maxThreads, int port)
  {
    super(maxThreads,port);
  }

  private int nfib(int n)
  {
    if (n <= 1)
      return 1;
    else
      return nfib(n-2)+nfib(n-1);
  }

  public void compute(int delay)
  {
    for (int i = 0; i < delay; i++)
      nfib(32);
  }

  public void process(InputStream cin, OutputStream cout) throws Exception
  {
    PrintWriter writer = new PrintWriter(cout);
    writer.print(".");
    writer.flush();

    Scanner scanner = new Scanner(cin);
    String line = scanner.nextLine();
    System.out.println(line);
    String[] tokens = line.split("\\s+");
    int value = Integer.parseInt(tokens[0]);
    int delay = Integer.parseInt(tokens[1]);
    long start = System.nanoTime();
    compute(delay);
    long end = System.nanoTime();
    int ms = (int)((end-start)/1000000);
    writer.print(value);
    writer.flush();
  }

  public static void main(String[] args) throws Exception
  {
    if (args.length == 0) {
      System.out.println("Please specify port number");
      System.exit(-1);
    }

    int port = Integer.parseInt(args[0]);
    Compute2 comp = new Compute2(3,port);
    comp.serve();
  }
}
