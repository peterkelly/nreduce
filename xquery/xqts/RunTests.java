import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import java.util.List;
import java.util.ArrayList;
import java.util.Timer;
import java.util.TimerTask;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.w3c.dom.Text;

public class RunTests
{
  public static final String XQTS_NAMESPACE = "http://www.w3.org/2005/02/query-test-XQTSCatalog";

  private Catalog catalog = null;
  private String xqDir = null;
  private String xqtsDir = null;
  private boolean verbose = false;
  private Timer timer = new Timer();
  private String singleName = null;

  public byte[] runProcess(String name, String[] cmdline, File wd)
    throws TestFailureException, IOException
  {
    final Thread mainThread = Thread.currentThread();
    WakeupTask wakeup = new WakeupTask(mainThread);
    try {
      Runtime rt = Runtime.getRuntime();

      Process p = rt.exec(cmdline,null,wd);
      ReaderThread outread = new ReaderThread(p.getInputStream());
      ReaderThread errread = new ReaderThread(p.getErrorStream());
      outread.start();
      errread.start();
      timer.schedule(wakeup,5000);
      try {
        int rc = p.waitFor();

        byte[] outdata = outread.getData();
        byte[] errdata = errread.getData();

        if (0 != rc) {
          if (verbose)
            printOutputError(outdata,errdata);
          throw new TestFailureException(name+": "+rc);
        }
        return outdata;
      }
      catch (InterruptedException e) {
        throw new TestFailureException(name+": timeout");
      }
    }
    finally {
      wakeup.cancel();
    }
  }

  public void dir(String group, Node parent)
  {
    for (Node n : Util.nlit(parent.getChildNodes())) {
      if (isXQTSElement(n,"test-group")) {
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
      if (verbose) {
        System.out.println("================= actual =====================");
        System.out.write(b1,0,b1.length);
        System.out.println();
        System.out.println("================ expected ====================");
        System.out.write(b2,0,b2.length);
        System.out.println();
        System.out.println("==============================================");
      }
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

  public void printStream(InputStream is)
    throws IOException
  {
    byte[] buf = new byte[1024];
    int r;
    int total = 0;
    while (0 <= (r = is.read(buf))) {
      System.out.write(buf,0,r);
      total++;
    }
    is.close();
  }

  public void printOutputError(byte[] out, byte[] err)
    throws IOException
  {
    System.out.println("************** standard output");
    System.out.write(out,0,out.length);
    System.out.println("************** standard error");
    System.out.write(err,0,err.length);
    System.out.println("**************");
  }

  public void singleTest(String name, String queryFile, String outputFile,
                      String compare)
    throws IOException, InterruptedException, TestFailureException
  {
    if (!(new File(queryFile)).exists())
      throw new FileNotFoundException(queryFile);

    File wd = new File(xqDir);
    String[] cmdline = {xqDir+"/runcompile-xqts",queryFile};
    byte[] outdata = runProcess("runcompile",cmdline,wd);

    String elcFile = xqDir+"/temp.elc";
    FileOutputStream os = new FileOutputStream(elcFile);
    os.write(outdata);
    cmdline = new String[]{"nreduce",elcFile};

    outdata = runProcess("nreduce",cmdline,wd);

    InputStream is = new ByteArrayInputStream(outdata);

    InputStream expectedIs = new FileInputStream(outputFile);
    if (!compare("-",is,expectedIs))
      throw new TestFailureException("comparison");
  }

  public void copyFile(File source, File dest)
    throws IOException
  {
    FileInputStream in = null;
    FileOutputStream out = null;

    try {
      in = new FileInputStream(source);
      out = new FileOutputStream(dest);

      byte[] buf = new byte[1024];
      int r;
      while (0 <= (r = in.read(buf))) {
        out.write(buf,0,r);
      }
    }
    finally {
      if (in != null)
        in.close();
      if (out != null)
        out.close();
    }
  }

  public void copySourceFiles(List<String> inputs, List<String> inputVariables)
    throws IOException
  {
    File externalDir = new File(xqDir+"/external");
    if (!externalDir.exists())
      externalDir.mkdir();
    for (int i = 0; i < inputs.size(); i++) {
      String variable = inputVariables.get(i);
      String relFilename = catalog.lookupSource(inputs.get(i));
      String absFilename = xqtsDir+"/"+relFilename;
      File source = new File(absFilename);
      String destFilename = xqDir+"/external/"+variable+".xml";
      File dest = new File(destFilename);
      copyFile(source,dest);
    }
  }

  public void cleanExternalDir()
    throws IOException
  {
    File externalDir = new File(xqDir+"/external");
    File[] entries = externalDir.listFiles();
    if (entries == null)
      throw new IOException("Invalid directory");
    for (File f : entries) {
      if (f.getName().endsWith(".xml")) {
        if (!f.delete())
          throw new IOException("Can't delete "+f.getName());
      }
    }
  }

  public boolean isXQTSElement(Node n, String localName)
  {
    return (n.getNodeType() == Node.ELEMENT_NODE) &&
      n.getLocalName().equals(localName) &&
      n.getNamespaceURI().equals(XQTS_NAMESPACE);
  }

  public void testCase(String group, Node n)
  {
    String name = ((Element)n).getAttribute("name");

    String filePath = ((Element)n).getAttribute("FilePath");
    String queryName = null;
    List<String> inputs = new ArrayList<String>();
    List<String> inputVariables = new ArrayList<String>();
    String output = null;
    String compare = null;
    for (Node c : Util.nlit(n.getChildNodes())) {
      if (isXQTSElement(c,"query"))
        queryName = ((Element)c).getAttribute("name");
      if (isXQTSElement(c,"input-file")) {
        inputVariables.add(((Element)c).getAttribute("variable"));
        inputs.add(c.getTextContent());
      }
      if (isXQTSElement(c,"output-file")) {
        compare = ((Element)c).getAttribute("compare");
        output = c.getTextContent();
      }
    }

    try {
      String queryFile = xqtsDir+"/Queries/XQuery/"+filePath+queryName+".xq";
      String outputFile = xqtsDir+"/ExpectedTestResults/"+filePath+output;

      if (verbose) {
        System.out.println("  name "+name);
        System.out.println("  queryName "+queryName);
        System.out.println("  file "+queryFile);
        System.out.println("  outputFile "+outputFile);
        System.out.println("  compare "+compare);
      }

      if (queryName == null)
        throw new TestFailureException("No query name");

      copySourceFiles(inputs,inputVariables);
      try {
        singleTest(name,queryFile,outputFile,compare);
      }
      finally {
        cleanExternalDir();
      }

      System.out.format("%-40s Passed\n",name);
    }
    catch (Exception e) {
      System.out.format("%-40s Failed (%s)\n",name,e.getMessage());
    }
  }

  public void recursiveTests(String group, Node parent, Set<String> groups)
    throws Exception
  {
    for (Node n : Util.nlit(parent.getChildNodes())) {
      if (isXQTSElement(n,"test-group")) {
        Element e = (Element)n;
        String newGroup = group+"/"+e.getAttribute("name");

        if (groups.contains(newGroup)) {
          System.out.println(newGroup);
          System.out.println("========================================"+
                             "========================================");
        }

        recursiveTests(newGroup,n,groups);
      }
      else if (isXQTSElement(n,"test-case")) {
        if (groups.contains(group)) {
          if ((singleName == null) ||
              ((Element)n).getAttribute("name").equals(singleName))
          testCase(group,n);
        }
      }
      else {
        recursiveTests(group,n,groups);
      }
    }
  }

  public void doTests(String groupsFile)
    throws Exception
  {
    Set<String> groups = readGroups(groupsFile);
    recursiveTests("",catalog.getDocument(),groups);
  }

  public void usage()
  {
      System.err.println("Usage: Runtests (-d|-r) options");
      System.err.println("Actions:");
      System.err.println("  -d              List groups in catalog (requires -c)");
      System.err.println("  -r              Run tests (requires -c,-g,-x)");
      System.err.println("Options:");
      System.err.println("  -v              Verbose");
      System.err.println("  -c <catalog>    Catalog file");
      System.err.println("  -g <groups>     Groups file");
      System.err.println("  -x <xqdir>      Location of XQuery compiler");
      System.err.println("  -n <name>       Only run single test <name>");
      System.exit(-1);
  }

  public void run(String[] args)
    throws Exception
  {

    int argno = 0;
    String catalogFilename = null;
    String groupsFilename = null;
    boolean dir = false;
    boolean run = false;

    while (argno < args.length) {
      if (args[argno].equals("-d"))
        dir = true;
      else if (args[argno].equals("-r"))
        run = true;
      else if (args[argno].equals("-v"))
        verbose = true;
      else if (args[argno].equals("-c") && (argno+1 < args.length))
        catalogFilename = args[++argno];
      else if (args[argno].equals("-g") && (argno+1 < args.length))
        groupsFilename = args[++argno];
      else if (args[argno].equals("-x") && (argno+1 < args.length))
        xqDir = args[++argno];
      else if (args[argno].equals("-n") && (argno+1 < args.length))
        singleName = args[++argno];
      argno++;
    }

    if (!dir && !run)
      usage();

    if (dir && run)
      usage();

    if (catalogFilename == null)
      usage();

    xqtsDir = new File(catalogFilename).getParent();
    catalog = new Catalog(catalogFilename);

    if (dir) {
      dir("",catalog.getDocument());
    }
    else {
      if ((groupsFilename == null) || (xqDir == null))
        usage();

      doTests(groupsFilename);
    }
  }

  public static void main(String[] args)
    throws Exception
  {
    new RunTests().run(args);
    System.exit(0);
  }
}
