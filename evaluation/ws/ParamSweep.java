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

@WebService(targetNamespace="urn:marks")
@SOAPBinding(style=SOAPBinding.Style.RPC,  
             use=SOAPBinding.Use.LITERAL)
public class ParamSweep
{
  private Random rand;

  public ParamSweep()
  {
  }

  @WebMethod
  public double run(int x, int y)
  {
    return 2*Math.sin(x+y) + 3*Math.sin(y);
    //    return 2*Math.cos(x) + 3*Math.sin(y) - Math.cos(y);
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

    Endpoint endpoint = Endpoint.create(new ParamSweep());
    endpoint.publish(server.createContext("/paramsweep"));

    HttpContext context = server.createContext("/");
    context.setHandler(new BasicHandler());

    System.out.println("ParamSweep service started");
  }
}
