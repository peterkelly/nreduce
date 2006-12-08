package client;

import java.net.URL;

import client.stubs.Calculator;
import client.stubs.CalculatorServiceLocator;

public class Client
{
  public static void main(String[] args)
  {
    try
    {
      CalculatorServiceLocator locator = new CalculatorServiceLocator();
      Calculator calc;
      if (args.length > 0)
        calc = locator.getcalculator(new URL(args[0]));
      else
        calc = locator.getcalculator();
      System.out.println("add = "+calc.add(4,5));
      System.out.println("multiply = "+calc.multiply(4,5));
      int[] range = calc.range(4,9);
      for (int i = 0; i < range.length; i++)
        System.out.println("range["+i+"] = "+range[i]);
    }
    catch (Exception e)
    {
      System.out.println(e);
    }
  }
}
