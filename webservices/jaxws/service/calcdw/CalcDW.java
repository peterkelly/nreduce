package service.calcdw;

import javax.jws.WebService;
import javax.jws.WebMethod;
import javax.jws.soap.SOAPBinding;

@WebService
@SOAPBinding(style=SOAPBinding.Style.DOCUMENT, 
             use=SOAPBinding.Use.LITERAL, 
             parameterStyle=SOAPBinding.ParameterStyle.WRAPPED)
public class CalcDW
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
