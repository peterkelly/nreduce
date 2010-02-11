import com.sun.net.httpserver.HttpContext;
import com.sun.net.httpserver.HttpServer;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.Executors;
import javax.jws.WebMethod;
import javax.jws.WebService;
import javax.jws.soap.SOAPBinding;
import javax.xml.ws.Endpoint;
import com.sun.net.httpserver.HttpHandler;

@WebService(targetNamespace="urn:temp")
@SOAPBinding(style=SOAPBinding.Style.RPC,  
             use=SOAPBinding.Use.LITERAL)
public class HR
{
  private Employee[] employees;

  public HR()
  {
    employees = new Employee[3];
    employees[0] = new Employee(12960,"Bob","Smith","Technician","Engineering",55000);
    employees[1] = new Employee(12823,"Gary","Johnson","Manager","Sales",65800);
    employees[2] = new Employee(12012,"Joe","Williams","Assistant","Sales",49250);
  }

  @WebMethod
  public int[] listEmployees()
  {
    int[] ids = new int[employees.length];
    for (int i = 0; i < ids.length; i++)
      ids[i] = employees[i].id;
    return ids;
  }

  @WebMethod
  public Employee employeeInfo(int id)
    throws Exception
  {
    for (int i = 0; i < employees.length; i++) {
      if (employees[i].id == id)
        return employees[i];
    }
    throw new Exception("Employee "+id+" not found");
  }

  public static void main(String[] args)
    throws IOException
  {
    if (args.length < 1) {
      System.err.println("Please specify port number");
      System.exit(1);
    }
    int port = Integer.parseInt(args[0]);

    System.setProperty("sun.net.httpserver.readTimeout","3600");

    HttpServer server = HttpServer.create(new InetSocketAddress(port), 5);
    server.setExecutor(Executors.newFixedThreadPool(3));
    server.start();

    Endpoint endpoint = Endpoint.create(new HR());
    HttpContext tempContext = server.createContext("/hr");
    endpoint.publish(tempContext);
    System.out.println("Endpoint's handler is "+
                       tempContext.getHandler().getClass().getName());

    HttpContext wrappedContext = server.createContext("/wrap");
    HttpHandler handler = tempContext.getHandler();
    wrappedContext.setHandler(new CloseHandler(handler));

    Map<String,Object> properties = endpoint.getProperties();
    for (Map.Entry<String,Object> entry : properties.entrySet()) {
      System.out.println("property "+entry.getKey()+" = "+entry.getValue());
    }
    System.out.println("Finished printing properties");

    HttpContext context = server.createContext("/");
    context.setHandler(new BasicHandler());

    System.out.println("HR service started");
  }
}
