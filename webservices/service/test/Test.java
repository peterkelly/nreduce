package service.test;

import javax.jws.WebService;
import javax.jws.WebMethod;
import javax.jws.soap.SOAPBinding;
import java.util.Random;

@WebService
@SOAPBinding(style=SOAPBinding.Style.RPC,  
             use=SOAPBinding.Use.LITERAL) 
public class Test
{

  private String[] categories = 
  {"Apparel and Accessories",
   "Automotive",
   "Baby",
   "Beauty",
   "Books",
   "Cell phones and service",
   "Electronics",
   "Everything else",
   "Gourmet food",
   "Grocery",
   "Health and personal care",
   "Home and garden",
   "Home improvement",
   "Industrial and scientific",
   "Jewelery",
   "Magazines",
   "Movies and TV",
   "Music",
   "Musical instruments",
   "Office products and supplies",
   "Shoes",
   "Software",
   "Sports and outdoors",
   "Toys and games",
   "VHS",
   "Video games",
   "Watches"};

  String[] locations = {
    "Aberfoyle Park",
    "Aldinga",
    "Aldinga Beach",
    "Blewitt Springs",
    "Chandlers Hill",
    "Cherry Gardens",
    "Christie Downs",
    "Christies Beach",
    "Clarendon",
    "Coromandel East",
    "Coromandel Valley",
    "Darlington",
    "Dorset Vale",
    "Flagstaff Hill",
    "Hackham",
    "Hackham West",
    "Happy Valley",
    "Huntfield Heights"};

  private Sale[] sales;

  public Test()
  {
    sales = new Sale[1000];
    Random rand = new Random(1);
    for (int i = 0; i < sales.length; i++) {
      sales[i] = new Sale();
      sales[i].custId = rand.nextInt(10);
      sales[i].category = categories[rand.nextInt(categories.length)];
      sales[i].location = locations[rand.nextInt(locations.length)];
      sales[i].time = rand.nextInt(10000000);
      sales[i].amount = 1+rand.nextInt(99);
    }
  }

  @WebMethod
  public String sayHello(String name)
  {
    return "Hello "+name;
  }

  @WebMethod
  public int add1(int value)
  {
    return value+1;
  }

  @WebMethod
  public int add1comp(int value, int compute)
  {
    for (int i = 0; i < compute; i++) {
      for (int j = 0; j < 1000000; j++) {
      }
    }
    return value+1;
  }

  @WebMethod
  public String other(String x)
  {
    return x;
  }

  @WebMethod
  public String[] getCategories()
  {
    return categories;
  }

  @WebMethod
  public String[] getLocations()
  {
    return locations;
  }

  @WebMethod
  public Sale[] getSalesData(String location)
  {
    System.out.println("getSalesData: location = \""+location+"\"");
    int count = 0;
    for (int i = 0; i < sales.length; i++) {
      if (sales[i].location.equals(location))
        count++;
    }
    System.out.println("count = "+count);
    Sale[] res = new Sale[count];
    int pos = 0;
    for (int i = 0; i < sales.length; i++) {
      if (sales[i].location.equals(location))
        res[pos++] = sales[i];
    }
    return res;
  }
}
