package client;

import java.net.URL;
import java.util.List;
import javax.xml.namespace.QName;

import proxy.calcdw.CalcDW;
import proxy.calcdw.CalcDWService;

public class CalcDWClient
{
    public static void main(String[] args)
        throws Exception
    {
        if (args.length < 1) {
            System.err.println("Usage: Please specify WSDL url\n");
            System.exit(1);
        }

        URL svcurl = new URL(args[0]);
        QName svcname = new QName("http://calcdw.service/", "CalcDWService");
        CalcDWService locator = new CalcDWService(svcurl,svcname);
        CalcDW svc = locator.getCalcDWPort();

        System.out.println("sqrt(2) = "+svc.sqrt(2));
        System.out.println("4 + 5 = "+svc.add(4,5));
        System.out.println("4 + 5 + 6 = "+svc.add3(4,5,6));
    }
}
