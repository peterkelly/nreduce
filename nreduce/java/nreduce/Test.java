/*
 * This file is part of the NReduce project
 * Copyright (C) 2007-2009 Peter Kelly <kellypmk@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * $Id$
 *
 */

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
