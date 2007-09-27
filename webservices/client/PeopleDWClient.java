package client;

import java.net.URL;
import java.util.List;
import javax.xml.namespace.QName;

import proxy.peopledw.PeopleDW;
import proxy.peopledw.PeopleDWService;
import proxy.peopledw.Person;

public class PeopleDWClient
{
    public static void main(String[] args)
        throws Exception
    {
        if (args.length < 1) {
            System.err.println("Usage: Please specify WSDL url\n");
            System.exit(1);
        }

        URL svcurl = new URL(args[0]);
        QName svcname = new QName("http://peopledw.service/", "PeopleDWService");
        PeopleDWService locator = new PeopleDWService(svcurl,svcname);
        PeopleDW svc = locator.getPeopleDWPort();

        System.out.println("Names:\n");
        List<String> names = svc.getNames();
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
        List<Person> people = svc.getAllPeople();
        for (Person person : people) {
            System.out.println("name = "+person.getName()+
                               ", age = "+person.getAge()+
                               ", occupation = "+person.getOccupation());
        }
        System.out.println();

        int total = svc.totalAges(people);
        System.out.println("total ages = "+total);
        System.out.println();

        String description = svc.describe(people.get(0));
        System.out.println("people[0] description = "+description);
    }

}
