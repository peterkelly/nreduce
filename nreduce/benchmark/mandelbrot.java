public class mandelbrot
{

    public static double mandel(double Cr, double Ci, double iterations)
    {
        double res;
        double Zr = 0;
        double Zi = 0;
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

    public static void printcell(double num)
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

    public static void mloop(double minx, double maxx, double xincr,
                             double miny, double maxy, double yincr)
    {
        double x;
        double y;
        for (y = miny; y < maxy; y += yincr) {
            for (x = minx; x < maxx; x += xincr) {
                printcell(mandel(x,y,4096));
            }
            System.out.print("\n");
        }
    }

    public static void mandelrange(double minx, double maxx, double xincr,
                                   double miny, double maxy, double yincr)
    {
        mloop(minx,maxx+xincr,xincr,miny,maxy,yincr);
    }

    public static void main(String[] args)
    {
        double incr = 0.01;
        if (1 <= args.length)
            incr = Double.parseDouble(args[0]);
        System.out.println("incr = "+incr);
        mandelrange(-1.5,0.5,incr,-1.0,1.0,incr);
    }

}
