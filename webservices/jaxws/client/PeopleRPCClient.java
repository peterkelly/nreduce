package client;

import java.net.URL;
import java.util.List;
import javax.xml.namespace.QName;

import proxy.peoplerpc.PeopleRPC;
import proxy.peoplerpc.PeopleRPCService;
import proxy.peoplerpc.Person;

public class PeopleRPCClient
{
    public static void main(String[] args)
        throws Exception
    {
        if (args.length < 1) {
            System.err.println("Usage: Please specify WSDL url\n");
            System.exit(1);
        }

        URL svcurl = new URL(args[0]);
        QName svcname = new QName("http://peoplerpc.service/", "PeopleRPCService");
        PeopleRPCService locator = new PeopleRPCService(svcurl,svcname);
        PeopleRPC svc = locator.getPeopleRPCPort();

        System.out.println("Names:\n");
        List<String> names = svc.getNames().getItem();
        for (int i = 0; i < names.size(); i++)
            System.out.println(names.get(i));
        System.out.println();

        System.out.println("Individual people:\n");
        for (String name : names) {
            Person p = svc.getPerson(name);
            System.out.println("name = "+p.getName()+
                               ", age = "+p.getAge()+
                               ", occupation = "+p.getOccupation());
        }
        System.out.println();

        System.out.println("All people:\n");
        List<Person> people = svc.getAllPeople().getItem();
        for (Person person : people) {
            System.out.println("name = "+person.getName()+
                               ", age = "+person.getAge()+
                               ", occupation = "+person.getOccupation());
        }
    }

}
