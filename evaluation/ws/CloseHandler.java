import java.io.IOException;
import java.io.InputStream;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.Headers;

public class CloseHandler implements HttpHandler
{
  private HttpHandler handler;

  public CloseHandler(HttpHandler handler)
  {
    this.handler = handler;
    System.out.println("CloseHandler: existing handler = "+handler.getClass().getName());
  }

  public void handle(HttpExchange exchange)
    throws IOException
  {
    handler.handle(exchange);
    System.out.println("CloseHandler: before closing connection");
    exchange.close();
    System.out.println("CloseHandler: after closing connection");
  }
}
