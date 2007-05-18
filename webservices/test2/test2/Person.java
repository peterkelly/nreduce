package test2;

import java.io.Serializable;

public class Person implements Serializable
{
    public String name;
    public int age;
    public String occupation;

    public Person()
    {
    }

    public Person(String name, int age, String occupation)
    {
	this.name = name;
	this.age = age;
	this.occupation = occupation;
    }

    public String getName()
    {
	return name;
    }

    public int getAge()
    {
	return age;
    }

    public String getOccupation()
    {
	return occupation;
    }

    public void setName(String name)
    {
	this.name = name;
    }

    public void setAge(int age)
    {
	this.age = age;
    }

    public void setOccupation(String occupation)
    {
	this.occupation = occupation;
    }
}
