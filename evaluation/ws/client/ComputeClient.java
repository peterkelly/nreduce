package client;

import java.net.URL;
import java.util.List;
import javax.xml.namespace.QName;

import proxy.compute.Compute;
import proxy.compute.ComputeService;

public class ComputeClient
{
    public static void main(String[] args)
        throws Exception
    {
        if (args.length < 1) {
            System.err.println("Usage: Please specify WSDL url\n");
            System.exit(1);
        }

        URL svcurl = new URL(args[0]);
        QName svcname = new QName("urn:compute", "ComputeService");
        ComputeService locator = new ComputeService(svcurl,svcname);
        Compute svc = locator.getComputePort();

        for (int i = 0; i < 10; i++) {
          System.out.println(svc.compute(123,1000));
        }
    }

}
