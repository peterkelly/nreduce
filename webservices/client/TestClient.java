package client;

import java.net.URL;
import java.util.List;
import javax.xml.namespace.QName;

import proxy.test.Test;
import proxy.test.TestService;

public class TestClient
{
    public static void main(String[] args)
        throws Exception
    {
        if (args.length < 1) {
            System.err.println("Usage: Please specify WSDL url\n");
            System.exit(1);
        }

        URL svcurl = new URL(args[0]);
        QName svcname = new QName("http://test.service/", "TestService");
        TestService locator = new TestService(svcurl,svcname);
        Test svc = locator.getTestPort();

        svc.add1Comp(1,1);
    }

}
