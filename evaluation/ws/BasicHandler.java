import java.io.IOException;
import java.io.InputStream;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.Headers;

// See: 
// http://java.sun.com/javase/6/docs/jre/api/net/httpserver/spec/index.html
// http://jtute.com/java6/0406.html

public class BasicHandler implements HttpHandler
{
  // Override default behaviour of built-in WS server, which doesn't
  // handle HEAD requests correctly
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
}
