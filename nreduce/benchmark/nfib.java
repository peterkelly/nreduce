public class nfib
{
  static int nfib(int n)
  {
    if (n <= 1)
      return 1;
    else
      return nfib(n-2)+nfib(n-1);
  }

  public static void main(String[] args)
  {
    if (args.length < 1) {
      System.err.println("no max value supplied");
      System.exit(-1);
    }

    int max = Integer.parseInt(args[0]);

    for (int i = 0; i <= max; i++)
      System.out.println("nfib("+i+") = "+nfib(i));
  }
}
