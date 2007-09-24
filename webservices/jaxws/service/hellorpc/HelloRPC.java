package service.hellorpc;

import javax.jws.WebService;
import javax.jws.WebMethod;
import javax.jws.soap.SOAPBinding;

@WebService
@SOAPBinding(style=SOAPBinding.Style.RPC,  
             use=SOAPBinding.Use.LITERAL) 
public class HelloRPC
{
    @WebMethod
    public String sayHello(String name)
    {
        return "Hello "+name;
    }
}
