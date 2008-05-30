import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.w3c.dom.Text;

public class RunTests
{
  public Iterable<Node> nlit(final NodeList nl)
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

  public void dir(String group, Node parent)
  {
    for (Node n : nlit(parent.getChildNodes())) {
      if (n.getNodeName().equals("test-group")) {
        Element e = (Element)n;
        String newGroup = group+"/"+e.getAttribute("name");
        System.out.println(newGroup);
        dir(newGroup,n);
      }
      else {
        dir(group,n);
      }
    }
  }

  Set<String> readGroups(String filename)
    throws IOException
  {
    Set<String> groups = new HashSet<String>();
    FileReader fread = new FileReader(filename);
    BufferedReader bread = new BufferedReader(fread);
    String line;
    while ((line = bread.readLine()) != null)
      groups.add(line);
    fread.close();
    return groups;
  }

  public Map<String,String> parseSources(Document doc)
  {
    Map<String,String> sources = new HashMap<String,String>();
    for (Node n1 : nlit(doc.getChildNodes())) {
      for (Node n2 : nlit(n1.getChildNodes())) {
        if (n2.getNodeName().equals("sources")) {
          for (Node n3 : nlit(n2.getChildNodes())) {
            if (n3.getNodeName().equals("source")) {
              String id = ((Element)n3).getAttribute("ID");
              String fileName = ((Element)n3).getAttribute("FileName");
              sources.put(id,fileName);
            }
          }
          return sources;
        }
      }
    }
    return sources;
  }

  public byte[] streamToByteArray(InputStream is)
    throws IOException
  {
    byte[] buf = new byte[1024];
    ByteArrayOutputStream bout = new ByteArrayOutputStream();
    int r;
    while (0 <= (r = is.read(buf))) {
      bout.write(buf,0,r);
    }
    is.close();
    return bout.toByteArray();
  }

  public boolean compare(String method, InputStream is1, InputStream is2)
    throws IOException
  {
    byte[] b1 = streamToByteArray(is1);
    byte[] b2 = streamToByteArray(is2);
    if (Arrays.equals(b1,b2)) {
      return true;
    }
    else {
//       System.out.println("================= actual =====================");
//       System.out.write(b1,0,b1.length);
//       System.out.println();
//       System.out.println("================ expected ====================");
//       System.out.write(b2,0,b2.length);
//       System.out.println();
//       System.out.println("==============================================");
      return false;
    }

    /*
    if (method.equals("xml")) {
    }
    else {
      throw new Exception("Unknown comparison method");
    }
    */
  }

  class TestFailureException extends Exception
  {
    public TestFailureException(String msg)
    {
      super(msg);
    }
  }

  public void singleTest(String name, String queryFile, String inputFile, String outputFile,
                      String compare)
    throws IOException, InterruptedException, TestFailureException
  {
    if (!(new File(queryFile)).exists())
      throw new FileNotFoundException(queryFile);
    if (!(new File(inputFile)).exists())
      throw new FileNotFoundException(inputFile);

    Runtime rt = Runtime.getRuntime();
    File wd = new File(xqDir);
    String[] cmdline = {xqDir+"/runcompile",queryFile};
    Process p = rt.exec(cmdline,null,wd);
    int rc = p.waitFor();

    if (0 != rc)
      throw new TestFailureException("runcompile: "+rc);

    InputStream is = p.getInputStream();
    String elcFile = xqDir+"/temp.elc";
    FileOutputStream os = new FileOutputStream(elcFile);

    byte[] buf = new byte[1024];
    int r;
    while (0 <= (r = is.read(buf)))
      os.write(buf,0,r);
    os.close();

    cmdline = new String[]{"nreduce",elcFile};
    p = rt.exec(cmdline,null,wd);
    rc = p.waitFor();

    if (0 != rc)
      throw new TestFailureException("nreduce: "+rc);

    is = p.getInputStream();

    InputStream expectedIs = new FileInputStream(outputFile);
    if (!compare("-",is,expectedIs))
      throw new TestFailureException("comparison");
  }

  public void recursiveTests(String group, Node parent, Set<String> groups,
                   Map<String,String> sources)
    throws Exception
  {
    for (Node n : nlit(parent.getChildNodes())) {
      if (n.getNodeName().equals("test-group")) {
        Element e = (Element)n;
        String newGroup = group+"/"+e.getAttribute("name");

        if (groups.contains(newGroup)) {
          System.out.println(newGroup);
          System.out.println("========================================"+
                             "========================================");
        }

        recursiveTests(newGroup,n,groups,sources);
      }
      else if (n.getNodeName().equals("test-case")) {
        if (groups.contains(group)) {
          String name = ((Element)n).getAttribute("name");

          String filePath = ((Element)n).getAttribute("FilePath");
          String queryName = null;
          String input = null;
          String variable = null;
          String output = null;
          String compare = null;
          for (Node c : nlit(n.getChildNodes())) {
            if (c.getNodeName().equals("query"))
              queryName = ((Element)c).getAttribute("name");
            if (c.getNodeName().equals("input-file")) {
              variable = ((Element)c).getAttribute("variable");
              input = c.getTextContent();
            }
            if (c.getNodeName().equals("output-file")) {
              compare = ((Element)c).getAttribute("compare");
              output = c.getTextContent();
            }
          }
          String queryFile = xqtsDir+"/Queries/XQuery/"+filePath+queryName+".xq";
          String outputFile = xqtsDir+"/ExpectedTestResults/"+filePath+output;
          String inputFile = xqtsDir+"/"+sources.get(input);

          if (verbose) {
            System.out.println("  query "+name);
            System.out.println("  file "+queryFile);
            System.out.println("  input "+inputFile);
            System.out.println("  outputFile "+outputFile);
            System.out.println("  compare "+compare);
          }

          try {
            singleTest(name,queryFile,inputFile,outputFile,compare);
            System.out.format("%-40s Passed\n",name);
          }
          catch (Exception e) {
            System.out.format("%-40s Failed (%s)\n",name,e.getMessage());
          }
        }
      }
      else {
        recursiveTests(group,n,groups,sources);
      }
    }
  }

  Document doc = null;
  String xqDir = null;
  String xqtsDir = null;
  boolean verbose = false;

  public void doTests(String groupsFile)
    throws Exception
  {
    Set<String> groups = readGroups(groupsFile);
    Map<String,String> sources = parseSources(doc);
    recursiveTests("",doc,groups,sources);
  }

  public void run(String[] args)
    throws Exception
  {
    if (args.length < 2) {
      System.err.println("Usage: Runtests ...options...");
      System.err.println("  -d <catalog>                  Directory of test groups");
      System.err.println("  -r <catalog> <groups> <xqDir> Runtests contains in groups file");
      System.exit(-1);
    }

    String action = args[0];
    String catalogFilename = args[1];
    DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
    DocumentBuilder builder = factory.newDocumentBuilder();
    doc = builder.parse(catalogFilename);

    xqtsDir = new File(catalogFilename).getParent();

    if (args[0].equals("-d")) {
      dir("",doc);
    }
    else if (args[0].equals("-r")) {
      String groupsFile = args[2];
      xqDir = args[3];
      doTests(groupsFile);
    }
    else {
      System.err.println("Invald option");
      System.exit(-1);
    }
  }

  public static void main(String[] args)
    throws Exception
  {
    new RunTests().run(args);
  }
}
