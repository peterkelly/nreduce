package client;

import java.net.URL;
import java.util.List;
import javax.xml.namespace.QName;

import proxy.hellodw.HelloDW;
import proxy.hellodw.HelloDWService;

public class HelloDWClient
{
    public static void main(String[] args)
        throws Exception
    {
        if (args.length < 1) {
            System.err.println("Usage: Please specify WSDL url\n");
            System.exit(1);
        }

        URL svcurl = new URL(args[0]);
        QName svcname = new QName("http://hellodw.service/", "HelloDWService");
        HelloDWService locator = new HelloDWService(svcurl,svcname);
        HelloDW svc = locator.getHelloDWPort();

        System.out.println(svc.sayHello("Peter"));
    }

}
