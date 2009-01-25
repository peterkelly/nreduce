package client;

import java.net.URL;
import java.util.List;
import javax.xml.namespace.QName;

import proxy.marks.Marks;
import proxy.marks.MarksService;
import proxy.marks.Student;
import proxy.marks.Test;

public class MarksClient
{
  public static void main(String[] args)
    throws Exception
  {
    if (args.length < 1) {
      System.err.println("Usage: Please specify WSDL url\n");
      System.exit(1);
    }

    URL svcurl = new URL(args[0]);
    QName svcname = new QName("urn:marks", "MarksService");
    MarksService locator = new MarksService(svcurl,svcname);
    Marks svc = locator.getMarksPort();

    System.out.println("Students:");
    List<Student> students = svc.getStudents().getItem();
    for (Student s : students) {
      System.out.println("name = "+s.getName()+
                         ", id = "+s.getId()+
                         ", degree = "+s.getDegree());
    }
    System.out.println();

    System.out.println("Tests:");
    List<Test> tests = svc.getTests().getItem();
    for (Test t : tests) {
      System.out.println("name = "+t.getName()+
                         ", marks = "+t.getMarks()+
                         ", input = "+t.getInput()+
                         ", expected = "+t.getExpected());
    }
    System.out.println();

    Student student = students.get(0);
    Test test = tests.get(0);
    String source = svc.getSource(student.getId());
    System.out.println("source = "+source);
    byte[] code = svc.compile(source);
    System.out.println("code = "+code);
    boolean result = svc.runTest(code,test.getInput(),test.getExpected());
    System.out.println("result = "+result);
  }

}
