import javax.xml.ws.Endpoint;
import javax.xml.ws.Binding;
import javax.xml.ws.handler.Handler;
import java.util.List;
import java.util.concurrent.Executors;
import java.net.InetSocketAddress;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import com.sun.net.httpserver.HttpServer;
import com.sun.net.httpserver.HttpContext;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.Headers;

// See: 
// http://java.sun.com/javase/6/docs/jre/api/net/httpserver/spec/index.html
// http://jtute.com/java6/0406.html

public class WebServices implements HttpHandler
{
  public void handle(HttpExchange exchange)
    throws IOException
  {
    InputStream in = exchange.getRequestBody();
    byte[] buf = new byte[256];
    int r;
    while (0 < (r = in.read(buf)))
      System.out.println("Read "+r+" bytes");
    System.out.println("Done reading");

    Headers headers = exchange.getResponseHeaders();
    headers.set("Content-Type","text/plain");
    headers.set("Content-Length","0");
    exchange.sendResponseHeaders(200,-1);
    exchange.close();
  }

  public static void main(String[] args)
    throws IOException
  {
    if (args.length < 1) {
      System.err.println("Please specify port number");
      System.exit(1);
    }
    int port = Integer.parseInt(args[0]);

    HttpServer server = HttpServer.create(new InetSocketAddress(port), 5);
    server.setExecutor(Executors.newFixedThreadPool(3));
    server.start();

    Endpoint endpoint = Endpoint.create(new Compute());
    endpoint.publish(server.createContext("/compute"));

    // Override default behaviour of built-in WS server, which doesn't
    // handle HEAD requests correctly
    HttpContext context = server.createContext("/");
    context.setHandler(new WebServices());

    System.out.println("WebServices started");
  }
}
