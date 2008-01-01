package client;

import java.net.URL;
import java.util.List;
import javax.xml.namespace.QName;

import proxy.mandelbrot.Mandelbrot;
import proxy.mandelbrot.MandelbrotService;

public class MandelbrotClient
{
    protected static void printcell(double num)
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

    public static void main(String[] args)
        throws Exception
    {
        if (args.length < 1) {
            System.err.println("Usage: Please specify WSDL url\n");
            System.exit(1);
        }

        URL svcurl = new URL(args[0]);
        QName svcname = new QName("http://mandelbrot.service/", "MandelbrotService");
        MandelbrotService locator = new MandelbrotService(svcurl,svcname);
        Mandelbrot svc = locator.getMandelbrotPort();

//         double incr = 0.1;
//         double average = svc.domandel(-1.5,0.5,-1.0,1.0,incr);
//         System.out.println("average = "+average);

        double minx = -1.5;
        double maxx = 0.5;
        double miny = -1.0;
        double maxy = 1.0;

        double incr = 0.1;

        double x;
        double y;
        double count2 = 0.0;
        double total2 = 0.0;
        for (y = miny; y <= maxy; y += incr) {
            for (x = minx; x <= maxx; x += incr) {
                double average = svc.domandel(x,x+incr,y,y+incr,incr/10);
//                 System.out.format("average(%f,%f) = %f\n",x,y,average);
                printcell(average);
                count2++;
                total2 += average;
            }
            System.out.println();
        }
        System.out.println("total2 = "+total2);
        double average2 = total2/count2;
        System.out.println("average2 = "+average2);
    }
}
