import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import java.util.Iterator;

public class Util
{
  public static Iterable<Node> nlit(final NodeList nl)
  {
    return new Iterable<Node>(){
      public Iterator<Node> iterator() {
        return new Iterator<Node>(){
          int index = 0;
          public boolean hasNext()
          {
            return nl.getLength() > index;
          }
          public Node next()
          {
            return nl.item(index++);
          }
          public void remove()
          {
          }
        };
      }
    };
  }

}
