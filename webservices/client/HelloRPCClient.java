package client;

import java.net.URL;
import java.util.List;
import javax.xml.namespace.QName;

import proxy.hellorpc.HelloRPC;
import proxy.hellorpc.HelloRPCService;

public class HelloRPCClient
{
    public static void main(String[] args)
        throws Exception
    {
        if (args.length < 1) {
            System.err.println("Usage: Please specify WSDL url\n");
            System.exit(1);
        }

        URL svcurl = new URL(args[0]);
        QName svcname = new QName("http://hellorpc.service/", "HelloRPCService");
        HelloRPCService locator = new HelloRPCService(svcurl,svcname);
        HelloRPC svc = locator.getHelloRPCPort();

        System.out.println(svc.sayHello("Peter"));
    }

}
