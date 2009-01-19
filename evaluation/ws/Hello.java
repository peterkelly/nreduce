import javax.jws.WebService;
import javax.jws.WebMethod;
import javax.jws.soap.SOAPBinding;

@WebService(targetNamespace="urn:hello")
@SOAPBinding(style=SOAPBinding.Style.RPC,  
             use=SOAPBinding.Use.LITERAL)
public class Hello
{
    @WebMethod
    public String sayHello(String name)
    {
        return "Hello "+name;
    }
}
