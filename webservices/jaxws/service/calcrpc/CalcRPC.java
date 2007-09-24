package service.calcrpc;

import javax.jws.WebService;
import javax.jws.WebMethod;
import javax.jws.soap.SOAPBinding;

@WebService
@SOAPBinding(style=SOAPBinding.Style.RPC,  
             use=SOAPBinding.Use.LITERAL) 
public class CalcRPC
{
    @WebMethod
    public double sqrt(double a)
    {
        return Math.sqrt(a);
    }

    @WebMethod
    public int add(int a, int b)
    {
        return a + b;
    }

    @WebMethod
    public int add3(int a, int b, int c)
    {
        return a + b + c;
    }
}
