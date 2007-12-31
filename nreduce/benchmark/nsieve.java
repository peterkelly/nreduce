// Based on the nsieve program from http://shootout.alioth.debian.org/ by Alexei Svitkine

public class nsieve
{
  static int nsieve(int n, boolean[] prime)
  {
    int count = 0;

    for (int i = 2; i <= n; i++)
      prime[i] = true;

    for (int i = 2; i <= n; i++) {
      if (prime[i]) {
        for (int k = i+i; k <= n; k += i)
          prime[k] = false;
        count++;
      }
    }
    return count;
  }

  public static void main(String[] args)
  {
    int n = 10000;
    if (args.length > 0)
      n = Integer.parseInt(args[0]);
    boolean[] prime = new boolean[n+1];
    System.out.println("Primes up to "+n+": "+nsieve(n,prime));
  }
}
