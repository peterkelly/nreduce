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
    int n = 24;
    if (args.length > 0)
      n = Integer.parseInt(args[0]);

    for (int i = 0; i <= n; i++)
      System.out.println("nfib("+i+") = "+nfib(i));
  }
}
