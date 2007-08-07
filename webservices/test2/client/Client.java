package client;

import java.net.URL;

import ws.Test2;
import ws.Test2ServiceLocator;
import ws.Person;

public class Client
{
    public static void main(String[] args)
        throws Exception
    {
        Test2ServiceLocator locator = new Test2ServiceLocator();
        Test2 svc;
        if (args.length > 0)
            svc = locator.gettest2(new URL(args[0]));
        else
            svc = locator.gettest2();

        System.out.println("Names:\n");
        String[] names = svc.getNames();
        for (int i = 0; i < names.length; i++)
            System.out.println(names[i]);
        System.out.println();

        System.out.println("Individual people:\n");
        for (int i = 0; i < names.length; i++) {
            Person p = svc.getPerson(names[i]);
            System.out.println("name = "+p.getName()+
                               ", age = "+p.getAge()+
                               ", occupation = "+p.getOccupation());
        }
        System.out.println();

        System.out.println("All people:\n");
        Person[] people = svc.getAllPeople();
        for (int i = 0; i < people.length; i++) {
            System.out.println("name = "+people[i].getName()+
                               ", age = "+people[i].getAge()+
                               ", occupation = "+people[i].getOccupation());
        }
    }

}
