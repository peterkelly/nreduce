package test2;

import javax.jws.WebService;
import javax.jws.WebMethod;
import javax.xml.ws.Endpoint;

@WebService
public class Test2
{
    static Person[] people = {
        new Person("Fred",20,"Plumber"),
        new Person("Joe",33,"Marketing executive"),
        new Person("Bob",38,"Builder")
    };


    @WebMethod
    public String[] getNames()
    {
        String[] names = new String[people.length];
        for (int i = 0; i < people.length; i++)
            names[i] = people[i].name;
        return names;
    }

    @WebMethod
    public Person getPerson(String name)
    {
        for (int i = 0; i < people.length; i++)
            if (people[i].name.equals(name))
                return people[i];
        return null;
    }

    @WebMethod
    public Person[] getAllPeople()
    {
        return people;
    }

    @WebMethod
    public String test()
    {
        return "Hello";
    }

    public static void main(String[] args)
    {
        Endpoint.publish("http://localhost:8080/test2",
                         new Test2());
    }
}
