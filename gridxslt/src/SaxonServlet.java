import net.sf.saxon.value.StringValue;
import net.sf.saxon.trans.DynamicError;
import net.sf.saxon.functions.SystemProperty;
import net.sf.saxon.trans.DynamicError;

import javax.servlet.ServletException;
import javax.servlet.ServletOutputStream;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.xml.transform.*;
import javax.xml.transform.stream.StreamResult;
import javax.xml.transform.stream.StreamSource;
import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.Enumeration;
import java.util.HashMap;

/**
 * SaxonServlet - apply a stylesheet to a given input file based on query
 * parameters.
 *
 * Based on the servlet example supplied with Saxon.
 *
 * This servlet is designed for use as part of the regression testing process,
 * when you want to run tests against saxon instead of gridxslt, to verify that
 * the tests themselves are correct (according to Saxon's behaviour). Rather
 * than running saxon itself, and incurring the JVM startup time for every
 * test, you can use saxonclient to access this servlet, so that the JVM stays
 * running continuously.
 */
public class SaxonServlet extends HttpServlet {

  public void init() throws ServletException {
    super.init();
    System.setProperty("javax.xml.transform.TransformerFactory",
                       "net.sf.saxon.TransformerFactoryImpl");
  }
    
  public void service(HttpServletRequest req, HttpServletResponse res)
    throws ServletException, IOException
  {
    String source = req.getParameter("source");
    String input = req.getParameter("input");

    ServletOutputStream out = res.getOutputStream();

    if (source == null) {
      out.println("No source parameter supplied");
      return;
    }

    if (input == null) {
      out.println("No input parameter supplied");
      return;
    }

    File sourceFile = new File(source);
    File inputFile = new File(input);

    if (!sourceFile.exists()) {
      out.println("Source file " + source + " does not exist");
      return;
    }

    if (!inputFile.exists()) {
      out.println("Input file " + input + " does not exist");
      return;
    }

    try {
      TransformerFactory factory = TransformerFactory.newInstance();
      Templates pss = factory.newTemplates(new StreamSource(sourceFile));
      Transformer transformer = pss.newTransformer();

      res.setContentType("text/plain");

      transformer.transform(new StreamSource(inputFile), new StreamResult(out));

    } catch (Exception err) {
      out.println(err.getMessage());
      err.printStackTrace(new PrintWriter(out));
    }
  }

  public String getServletInfo() {
    return "Saxon service";
  }

}
