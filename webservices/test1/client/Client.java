package client;

import java.net.URL;

import ws.Test1;
import ws.Test1ServiceLocator;

public class Client
{
    public static void main(String[] args)
	throws Exception
    {
	Test1ServiceLocator locator = new Test1ServiceLocator();
	Test1 svc;
	if (args.length > 0)
	    svc = locator.gettest1(new URL(args[0]));
	else
	    svc = locator.gettest1();
	System.out.println(svc.sayHello("Peter"));
	System.out.println(svc.add(4,5));
    }

}
