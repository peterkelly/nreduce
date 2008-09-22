package service.store;

import java.io.Serializable;

public class Product implements Serializable
{
  public int id;
  public String name;
  public String category;
  public int cost;

  public Product()
  {
  }

  public Product(int id, String name, String category, int cost)
  {
    this.id = id;
    this.name = name;
    this.category = category;
    this.cost = cost;
  }
}
