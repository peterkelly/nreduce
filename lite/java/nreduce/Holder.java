package nreduce;

public class Holder
{
    public Object obj;

    public Holder(Object obj)
    {
        this.obj = obj;
    }

    public Object getObj()
    {
        return obj;
    }

    public boolean equals(Object obj)
    {
        return (obj == this);
    }
}
