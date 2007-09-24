package service.hellodw;

import javax.jws.WebService;
import javax.jws.WebMethod;
import javax.jws.soap.SOAPBinding;

@WebService
@SOAPBinding(style=SOAPBinding.Style.DOCUMENT, 
             use=SOAPBinding.Use.LITERAL, 
             parameterStyle=SOAPBinding.ParameterStyle.WRAPPED)
public class HelloDW
{
    @WebMethod
    public String sayHello(String name)
    {
        return "Hello "+name;
    }
}
