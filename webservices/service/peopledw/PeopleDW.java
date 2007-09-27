package service.peopledw;

import javax.jws.WebService;
import javax.jws.WebMethod;
import javax.jws.soap.SOAPBinding;

import service.Person;

@WebService
@SOAPBinding(style=SOAPBinding.Style.DOCUMENT, 
             use=SOAPBinding.Use.LITERAL, 
             parameterStyle=SOAPBinding.ParameterStyle.WRAPPED)
public class PeopleDW
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
    public int totalAges(Person[] p)
    {
        int total = 0;
        for (int i = 0; i < p.length; i++)
            total += p[i].age;
        return total;
    }

    @WebMethod
    public String describe(Person p)
    {
        return p.name+" ("+p.age+"), "+p.occupation;
    }
}
