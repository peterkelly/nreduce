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
public class Marks
{
  private int nstudents;
  private int ntests;
  private int sourcesize;
  private int codesize;
  private int inoutsize;
  private int testms;

  private Student[] students;
  private Test[] tests;
  private String source;
  private byte[] code;
  private String inout;

  private Random rand;

  private Compute comp = new Compute();

  public Marks()
  {
    nstudents = setOption("nstudents",2);
    ntests = setOption("ntests",3);
    sourcesize = setOption("sourcesize",10);
    codesize = setOption("codesize",10);
    inoutsize = setOption("inoutsize",10);
    testms = setOption("testms",1000);
    rand = new Random(System.currentTimeMillis());

    // Students
    students = new Student[nstudents];
    for (int i = 0; i < students.length; i++)
      students[i] = new Student("Student "+i,""+i,"Degree");

    // Source
    char[] sourceChars = new char[sourcesize];
    for (int i = 0; i < sourceChars.length; i++)
      sourceChars[i] = (char)('A'+(i%26));
    source = new String(sourceChars);

    // Code
    code = new byte[codesize];
    for (int i = 0; i < code.length; i++)
      code[i] = (byte)i;

    // Input/output
    char[] inoutChars = new char[inoutsize];
    for (int i = 0; i < inoutChars.length; i++)
      inoutChars[i] = (char)('A'+(i%26));
    inout = new String(inoutChars);

    // Tests
    tests = new Test[ntests];
    for (int i = 0; i < ntests; i++)
      tests[i] = new Test("Test "+i,i+1,inout,inout);
  }

  private int setOption(String name, int def)
  {
    String envKey = name.toUpperCase();
    if (System.getenv(envKey) != null) {
      int value = Integer.parseInt(System.getenv(envKey));
      System.out.println(name+" = "+value);
      return value;
    }
    else {
      System.out.println(name+" = "+def+" (default)");
      return def;
    }
  }

  @WebMethod
  public Student[] getStudents()
  {
    return students;
  }

  @WebMethod
  public Test[] getTests()
  {
    return tests;
  }

  @WebMethod
  public String getSource(String student)
  {
    return source;
  }

  @WebMethod
  public byte[] compile(String source)
  {
    return code;
  }

  @WebMethod
  public boolean runTest(byte[] code, String input, String output)
  {
    comp.compute(0,testms);
    return (rand.nextDouble() >= 0.5);
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

    Endpoint endpoint = Endpoint.create(new Marks());
    endpoint.publish(server.createContext("/marks"));

    HttpContext context = server.createContext("/");
    context.setHandler(new BasicHandler());

    System.out.println("Marks service started");
  }
}
