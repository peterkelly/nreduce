package client;

import java.net.URL;
import java.util.List;
import javax.xml.namespace.QName;

import proxy.hello.Hello;
import proxy.hello.HelloService;

public class HelloClient
{
    public static void main(String[] args)
        throws Exception
    {
        if (args.length < 1) {
            System.err.println("Usage: Please specify WSDL url\n");
            System.exit(1);
        }

        URL svcurl = new URL(args[0]);
        QName svcname = new QName("urn:hello", "HelloService");
        HelloService locator = new HelloService(svcurl,svcname);
        Hello svc = locator.getHelloPort();

        System.out.println(svc.sayHello("Peter"));
    }

}
