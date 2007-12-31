public class mandelbrot
{
  static double mandel(double Cr, double Ci)
  {
    double res;
    double Zr = 0.0;
    double Zi = 0.0;
    for (res = 0; res < 4096; res++) {
      double newZr = ((Zr * Zr) - (Zi * Zi)) + Cr;
      double newZi = (2.0 * (Zr * Zi)) + Ci;
      double mag = Math.sqrt((newZr * newZr) + (newZi * newZi));
      if (mag > 2.0)
        return res;
      Zr = newZr;
      Zi = newZi;
    }
    return res;
  }

  static void printcell(double num)
  {
    if (num > 40)
      System.out.print("  ");
    else if (num > 30)
      System.out.print("''");
    else if (num > 20)
      System.out.print("--");
    else if (num > 10)
      System.out.print("**");
    else
      System.out.print("##");
  }

  static void mloop(double minx, double maxx, double miny, double maxy, double incr)
  {
    double x;
    double y;
    for (y = miny; y < maxy; y += incr) {
      for (x = minx; x < maxx; x += incr)
        printcell(mandel(x,y));
      System.out.println();
    }
  }

  public static void main(String[] args)
  {
    int n = 32;
    if (args.length > 0)
      n = Integer.parseInt(args[0]);

    double incr = 2.0/((double)n);
    mloop(-1.5,0.5,-1.0,1.0,incr);
  }
}
