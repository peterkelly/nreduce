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
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.ParserConfigurationException;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.Text;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;

import javax.xml.transform.TransformerFactory;
import javax.xml.transform.Transformer;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamResult;
import javax.xml.transform.TransformerConfigurationException;
import javax.xml.transform.TransformerException;
import java.io.PrintStream;

@WebService(targetNamespace="urn:marks")
@SOAPBinding(style=SOAPBinding.Style.RPC,  
             use=SOAPBinding.Use.LITERAL)
public class Forestry
{
  private int steps = 32;
  private double minlat = 5.26;
  private double maxlat = 8.82;
  private double minlong = 85.38;
  private double maxlong = 88.94;
  private int[][] map;

  public Forestry(int[][] map)
  {
    this.map = map;
  }

  @WebMethod
  public double analyze(double lat1, double long1,
                        double lat2, double long2,
                        int year)
  {
    int row = (int)(31*(lat1-minlat)/(maxlat-minlat));
    int col = (int)(31*(long1-minlong)/(maxlong-minlong));

    System.out.format("analyze %.3f,%.3f - %.3f,%.3f, %d, row %d col %d\n",
                      lat1,long1,lat2,long2,year,row,col);

    if (year <= 2004)
      return map[row][col];
    else
      return 0;
  }

  public static int[] getCols(Element parent)
  {
    NodeList children = parent.getChildNodes();
    int[] cols = new int[children.getLength()];
    for (int i = 0; i < children.getLength(); i++) {
      Node child = children.item(i);
      cols[i] = Integer.parseInt(child.getFirstChild().getNodeValue());
    }
    return cols;
  }

  public static int[][] getRows(Element parent)
  {
    NodeList children = parent.getChildNodes();
    int[][] rows = new int[children.getLength()][];
    for (int i = 0; i < children.getLength(); i++) {
      Node child = children.item(i);
      rows[i] = getCols((Element)child);
    }
    return rows;
  }

  public static void scaleValues(int[][] values)
  {
    int maxVal = 0;
    int minVal = Integer.MAX_VALUE;
    for (int row = 0; row < values.length; row++) {
      for (int col = 0; col < values[row].length; col++) {
        maxVal = Math.max(maxVal,values[row][col]);
        minVal = Math.min(minVal,values[row][col]);
      }
    }
    System.out.println("minVal = "+minVal);
    System.out.println("maxVal = "+maxVal);
    for (int row = 0; row < values.length; row++) {
      for (int col = 0; col < values[row].length; col++) {
        values[row][col] = 99*values[row][col]/maxVal;
      }
    }
  }

  public static void printValues(int[][] values)
  {
    for (int row = 0; row < values.length; row++) {
      for (int col = 0; col < values[row].length; col++) {
        System.out.format("%-3d ",values[row][col]);
      }
      System.out.println();
    }
  }

  public static Document readXML(String filename)
    throws SAXException, IOException
  {
    try {
      DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
      DocumentBuilder builder = factory.newDocumentBuilder();
      return builder.parse(filename);
    }
    catch (ParserConfigurationException e) {
      throw new RuntimeException(e);
    }
  }

  public static void main(String[] args)
    throws IOException, SAXException
  {
    if (args.length < 2) {
      System.err.println("Please specify port number and map filename");
      System.exit(1);
    }
    int port = Integer.parseInt(args[0]);
    String mapFilename = args[1];

    System.out.println("map filename = "+mapFilename);

    Document doc = readXML(mapFilename);

    System.out.println("Parsed map file");
    Element elem = doc.getDocumentElement();
    XMLUtil.removeWhitespace(elem);

    int values[][] = getRows(elem);

//     scaleValues(values);
    printValues(values);
//     System.out.println(doc);

//     writeXML(doc,System.out);

    System.setProperty("sun.net.httpserver.readTimeout","3600");

    HttpServer server = HttpServer.create(new InetSocketAddress(port), 5);
    server.setExecutor(Executors.newFixedThreadPool(3));

    Endpoint endpoint = Endpoint.create(new Forestry(values));
    endpoint.publish(server.createContext("/forestry"));

    HttpContext context = server.createContext("/");
    context.setHandler(new BasicHandler());

    server.start();
    System.out.println("Forestry service started");
  }
}
