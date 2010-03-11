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

  private static Compute comp;

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
    String outFail = inout.toLowerCase();

    // Tests - every second one fails
    tests = new Test[ntests];
    for (int i = 0; i < ntests; i++) {
      if (i % 2 == 0)
        tests[i] = new Test("Test "+i,i+1,inout,inout); // will pass
      else
        tests[i] = new Test("Test "+i,i+1,inout,outFail); // will fail
    }
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

    // If the first character is uppercase 'A', we treat it as a pass.
    // Otherwise we consider it a failure.
    // This allows us to get deterministic results so that we can validate
    // the output of workflows
    return (output.charAt(0) == 'A');
  }

  public static void main(String[] args)
    throws IOException
  {
    if (args.length < 1) {
      System.err.println("Please specify port number");
      System.exit(1);
    }
    int port = Integer.parseInt(args[0]);

    comp = new Compute(); // does calibration

    System.setProperty("sun.net.httpserver.readTimeout","3600");

    HttpServer server = HttpServer.create(new InetSocketAddress(port), 5);
    server.setExecutor(Executors.newFixedThreadPool(3));

    Endpoint endpoint = Endpoint.create(new Marks());
    endpoint.publish(server.createContext("/marks"));

    HttpContext context = server.createContext("/");
    context.setHandler(new BasicHandler());

    server.start();
    System.out.println("Marks service started");
  }
}
