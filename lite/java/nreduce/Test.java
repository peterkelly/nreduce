package nreduce;

import java.util.List;
import java.util.ArrayList;

public class Test
{
    int value = 0;

    public Test()
    {
    }

    public Test(int value)
    {
        this.value = value;
    }

    public int getValue()
    {
        return value;
    }

    public void adjustValue(int by)
    {
        value += by;
    }

    // static methods
    public static String sayHello(String name)
    {
        return "Hello "+name;
    }

    public static String concat(String one, String two)
    {
        return one+two;
    }

    public static Test combine(Test a, Test b)
    {
        return new Test(a.getValue()+b.getValue());
    }

    public static byte byteadd(byte a, byte b)
    {
        return (byte)(a + b);
    }

    public static short shortadd(short a, short b)
    {
        return (short)(a + b);
    }

    public static int intadd(int a, int b)
    {
        return a + b;
    }

    public static long longadd(long a, long b)
    {
        return a + b;
    }

    public static float floatadd(float a, float b)
    {
        return a + b;
    }

    public static double doubleadd(double a, double b)
    {
        return a + b;
    }

    public static boolean isPositive(int val)
    {
        return (val > 0);
    }

    public static boolean not(boolean val)
    {
        return !val;
    }

    public static void voidMethod()
    {
    }

    public static List createObject(String name)
    {
        return new ArrayList();
    }
}
