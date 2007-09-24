package service;

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
}
