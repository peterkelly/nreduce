package service.mandelbrot;

import javax.jws.WebService;
import javax.jws.WebMethod;
import javax.jws.soap.SOAPBinding;

@WebService
@SOAPBinding(style=SOAPBinding.Style.RPC,  
             use=SOAPBinding.Use.LITERAL) 
public class Mandelbrot
{
    protected double mandel(double Cr, double Ci, double iterations)
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

    protected void printcell(double num)
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

    @WebMethod
    public double domandel(double minx, double maxx,
                           double miny, double maxy, double incr)
    {
        System.out.format("domandel minx=%f, maxx=%f, miny=%f, maxy=%f, incr=%f\n",
                          minx,maxx,miny,maxy,incr);

        if (maxx < minx)
            System.out.println("ERROR! maxx < minx!");
        if (maxy < miny)
            System.out.println("ERROR! maxy < miny!");

        double x;
        double y;
        double count = 0.0;
        double total = 0.0;
        maxx += incr;
        for (y = miny; y < maxy; y += incr) {
            for (x = minx; x < maxx; x += incr) {
                double iterations = mandel(x,y,4096);
                 printcell(iterations);
                count++;
                total += iterations;
            }
             System.out.print("\n");
        }
        double average = total/count;
        System.out.println("average = "+average);
        return average;
    }
}
