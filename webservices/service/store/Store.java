package service.store;

import javax.jws.WebService;
import javax.jws.WebMethod;
import javax.jws.soap.SOAPBinding;

@WebService
  @SOAPBinding(style=SOAPBinding.Style.RPC,  
               use=SOAPBinding.Use.LITERAL) 
public class Store
{
  Product[] products = null;
  String[] categories = null;

  public Store()
  {
    int ncategories = 7;
    int nproducts = 100;

    categories = new String[ncategories];
    for (int i = 0; i < ncategories; i++)
      categories[i] = "Category"+i;

    products = new Product[nproducts];
    for (int i = 0; i < nproducts; i++)
      products[i] = new Product(i,"Name"+i,categories[i%7],(i+1)%100);
  }

  @WebMethod
  public String[] getCategories()
  {
    return categories;
  }

  @WebMethod
  public int[] getIds()
  {
    int[] ids = new int[products.length];
    for (int i = 0; i < products.length; i++)
      ids[i] = products[i].id;
    return ids;
  }

  @WebMethod
  public Product getProduct(int id)
  {
    return products[id];
  }
}
