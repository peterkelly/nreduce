package service;

import javax.xml.ws.Endpoint;
import service.peoplerpc.PeopleRPC;
import service.peopledw.PeopleDW;
import service.hellorpc.HelloRPC;
import service.hellodw.HelloDW;

public class RunServices
{
    public static void main(String[] args)
    {
        if (args.length < 1) {
            System.err.println("Please specify port number");
            System.exit(1);
        }
        int port = Integer.parseInt(args[0]);
        String base = "http://localhost:"+port;
        Endpoint.publish(base+"/hellorpc",new HelloRPC());
        Endpoint.publish(base+"/hellodw",new HelloDW());
        Endpoint.publish(base+"/peoplerpc",new PeopleRPC());
        Endpoint.publish(base+"/peopledw",new PeopleDW());
    }
}
