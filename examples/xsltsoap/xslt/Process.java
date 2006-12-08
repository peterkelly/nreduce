package xslt;

import java.io.*;
import javax.servlet.*;
import javax.servlet.http.*;
import javax.xml.transform.*;
import javax.xml.transform.stream.*;

public class Process extends HttpServlet {

  public void doPost(HttpServletRequest request,
                    HttpServletResponse response)
    throws ServletException, IOException {
    InputStream in = request.getInputStream();
    OutputStream out = response.getOutputStream();

    Source inSource = new StreamSource(in);
    response.setContentType("text/xml");
    process(request,inSource,out);
  }


  protected void process(HttpServletRequest request, Source inSource, OutputStream out)
    throws IOException
  {
    try {
      InputStream stream = getServletContext().getResourceAsStream(request.getServletPath());

      Source source = new StreamSource(stream);

      Result outResult = new StreamResult(out);

      TransformerFactory factory = TransformerFactory.newInstance();
      Transformer transformer = factory.newTransformer(source);
      transformer.transform(inSource,outResult);
    }
    catch (Exception e) {
      Writer writer = new OutputStreamWriter(out);
      writer.write(e+"\n");
      writer.flush();
    }
  }

}
