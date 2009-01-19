import javax.jws.WebService;
import javax.jws.WebMethod;
import javax.jws.soap.SOAPBinding;
import java.io.IOException;
import java.io.BufferedReader;
import java.io.InputStreamReader;

@WebService(targetNamespace="urn:compute")
@SOAPBinding(style=SOAPBinding.Style.RPC,  
             use=SOAPBinding.Use.LITERAL)
public class Compute
{
    @WebMethod
    public int compute(int value, int ms)
    {
      System.out.println("Starting compute "+value+" "+ms);
      String[] args = {"/bin/bash","-c","compute "+ms};
      try {
        Process proc = Runtime.getRuntime().exec(args);


        BufferedReader bread = new BufferedReader(new InputStreamReader(proc.getInputStream()));
        String line;
        while ((line = bread.readLine()) != null)
          System.out.println(line);

        bread = new BufferedReader(new InputStreamReader(proc.getErrorStream()));
        while ((line = bread.readLine()) != null)
          System.out.println(line);

        int rc = proc.waitFor();


        if (rc != 0) {
          System.err.println("compute exited with status "+rc);
          return -1;
        }


      } catch (IOException e) {
        e.printStackTrace();
        return -1;
      }
      catch (InterruptedException e) {
        e.printStackTrace();
        return -1;
      }

      return value;
    }
}
