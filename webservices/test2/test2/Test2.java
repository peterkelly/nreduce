package test2;

public class Test2
{
    static Person[] people = {
	new Person("Fred",20,"Plumber"),
	new Person("Joe",33,"Marketing executive"),
	new Person("Bob",38,"Builder")
    };


    public String[] getNames()
    {
	String[] names = new String[people.length];
	for (int i = 0; i < people.length; i++)
	    names[i] = people[i].name;
	return names;
    }

    public Person getPerson(String name)
    {
	for (int i = 0; i < people.length; i++)
	    if (people[i].name.equals(name))
		return people[i];
	return null;
    }

    public Person[] getAllPeople()
    {
	return people;
    }

    public String test()
    {
	return "Hello";
    }
}
