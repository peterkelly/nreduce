import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.w3c.dom.Text;
import org.xml.sax.SAXException;

public class Catalog
{
  private Document doc = null;
  private Map<String,String> sources = null;

  public Catalog(String filename)
    throws ParserConfigurationException, SAXException, IOException
  {
    DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
    factory.setNamespaceAware(true);
    DocumentBuilder builder = factory.newDocumentBuilder();
    doc = builder.parse(filename);
  }

  private void initSources()
  {
    if (sources != null)
      return;

    sources = new HashMap<String,String>();
    for (Node n1 : Util.nlit(doc.getChildNodes())) {
      for (Node n2 : Util.nlit(n1.getChildNodes())) {
        if (n2.getNodeName().equals("sources")) {
          for (Node n3 : Util.nlit(n2.getChildNodes())) {
            if (n3.getNodeName().equals("source")) {
              String id = ((Element)n3).getAttribute("ID");
              String fileName = ((Element)n3).getAttribute("FileName");
              sources.put(id,fileName);
            }
          }
          return;
        }
      }
    }
  }

  public String lookupSource(String ident)
  {
    initSources();
    return sources.get(ident);
  }


  public Document getDocument()
  {
    return doc;
  }
}
