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
import java.io.IOException;

public class XMLUtil
{
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

  public static void writeXML(Document doc, PrintStream out)
  {
    try {
      TransformerFactory tfac = TransformerFactory.newInstance();
      Transformer transformer = tfac.newTransformer();
      DOMSource source = new DOMSource(doc);
      StreamResult result = new StreamResult(System.out);
      transformer.transform(source,result);
    }
    catch (TransformerConfigurationException e) {
      throw new RuntimeException(e);
    }
    catch (TransformerException e) {
      throw new RuntimeException(e);
    }
  }

  public static void removeWhitespace(Element parent)
  {
    NodeList children = parent.getChildNodes();
    int i = 0;
    while (i < children.getLength()) {
      Node child = children.item(i);
      if ((child instanceof Text) && isWhitespace(child.getNodeValue()))
        parent.removeChild(child);
      else
        i++;
      if (child instanceof Element)
        removeWhitespace((Element)child);
    }
  }

  public static boolean isWhitespace(String str)
  {
    for (int i = 0; i < str.length(); i++) {
      if (!Character.isWhitespace(str.charAt(i)))
        return false;
    }
    return true;
  }
}
