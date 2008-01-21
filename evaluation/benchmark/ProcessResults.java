import java.util.*;
import java.io.*;

public class ProcessResults
{
  SortedMap<String,Program> programs = new TreeMap<String,Program>();

  final static int REAL = 0;
  final static int USER = 1;
  final static int SYS = 2;

  class Program
  {
    String name;
    SortedMap<Integer,Test> tests = new TreeMap<Integer,Test>();

    public Program(String name)
    {
      this.name = name;
    }

    public Test getTest(int n)
    {
      Test t = tests.get(n);
      if (t == null) {
        t = new Test(this,n);
        tests.put(n,t);
      }
      return t;
    }

    public void print(PrintWriter out)
    {
      out.println();
      out.println(name);
      for (Test test : tests.values())
        test.print(out);
    }

    public void dataNvsUser(String baseFilename)
      throws IOException
    {
      PrintWriter dataOut = new PrintWriter(baseFilename+".dat");
      PrintWriter plotOut = new PrintWriter(baseFilename+".plot");

      List<String> languages = new ArrayList<String>();

      Test ft = tests.values().iterator().next();
      dataOut.format("# %-10s ",name);
      for (Implementation impl : ft.impls.values()) {
        dataOut.format("%-8s ",impl.language);
        languages.add(impl.language);
      }
      dataOut.println();

      for (Test test : tests.values()) {
        dataOut.format("%-12d ",test.n);
        for (String language : languages) {
          Implementation impl = test.impls.get(language);
          dataOut.format("%-8.3f ",impl.avg[USER]);
        }
        dataOut.println();
      }

      plotOut.println("set terminal postscript eps color");
      plotOut.println("set out \""+baseFilename+".eps\"");
      plotOut.println("set key left");
      plotOut.println("set xlabel \"n\"");
      plotOut.println("set ylabel \"User time (s)\"");
      plotOut.println("set title \""+name+"\"");
      plotOut.println("set size 0.75,0.75");
      plotOut.println("plot \\");

      String options = " with lines linewidth 2";

      for (int i = 0; i < languages.size(); i++) {
        String language = languages.get(i);
        plotOut.print("  \""+baseFilename+".dat\" using 1:"+(i+2)+options+" title \""+
                        language+"\"");
        if (i+1 < languages.size())
          plotOut.print(", \\");
        plotOut.println();
      }

      dataOut.close();
      plotOut.close();

      runGnuplot(baseFilename);
    }
  }

  void programHist(String baseFilename)
    throws IOException
  {
    PrintWriter dataOut = new PrintWriter(baseFilename+".dat");
    PrintWriter plotOut = new PrintWriter(baseFilename+".plot");

    String[] languages = getCommonLanguages().toArray(new String[]{});
    dataOut.format("%-20s ","# program");
    for (String lang : languages)
      dataOut.format("%-8s ",lang);
    dataOut.println();

    for (Program p : programs.values()) {
      Test[] pt = p.tests.values().toArray(new Test[]{});
      Test last = pt[pt.length-1];
      dataOut.format("%-30s ","\""+p.name+"\\n"+last.n+"\"");
      for (String lang : languages) {
        Implementation impl = last.impls.get(lang);
        if (null == impl) {
          System.out.println(p.name+": no implementation for "+lang);
        }
        dataOut.format("%-8.3f ",impl.avg[USER]);
      }
      dataOut.println();
    }

    plotOut.println("set terminal postscript eps color");
    plotOut.println("set out \""+baseFilename+".eps\"");
    plotOut.println("set key left");
    plotOut.println("set ylabel \"User time (s)\"");
    plotOut.println("set title \"All programs\"");
    plotOut.println("set style data histograms");
    plotOut.println("set style fill solid 1.0 border -1");
    plotOut.println("set xtics rotate by 90");

    plotOut.println("plot \\");
    for (int i = 0; i < languages.length; i++) {
      plotOut.print("  \""+baseFilename+".dat\" using "+(i+2)+" : xticlabels(1) title \""+
                    languages[i]+"\"");
      if (i+1 < languages.length)
        plotOut.print(", \\");
      plotOut.println();
    }

    dataOut.close();
    plotOut.close();

    runGnuplot(baseFilename);
  }

  void runGnuplot(String baseFilename)
    throws IOException
  {
    String[] cmds = {"gnuplot",baseFilename+".plot"};
    Process proc = Runtime.getRuntime().exec(cmds);
    try {
      int rc = proc.waitFor();
      System.out.println("gnuplot "+baseFilename+".plot exited with return code "+rc);
    }
    catch (InterruptedException ex) {
      System.out.println("Interrupted");
    }
  }

  class Test
  {
    int n;
    Program prog;
    SortedMap<String,Implementation> impls = new TreeMap<String,Implementation>();

    public Test(Program prog, int n)
    {
      this.prog = prog;
      this.n = n;
    }

    public Implementation getImplementation(String language)
    {
      Implementation impl = impls.get(language);
      if (impl == null) {
        impl = new Implementation(this,language);
        impls.put(language,impl);
      }
      return impl;
    }

    public void print(PrintWriter out)
    {
      out.println("  "+n);
      for (Implementation impl : impls.values())
        impl.print(out);
    }
  }

  class Implementation
  {
    Test test;
    String language;
    List<Run> runs = new ArrayList<Run>();
    double[] min;
    double[] max;
    double[] avg;
    boolean computed = false;

    public Implementation(Test test, String language)
    {
      this.test = test;
      this.language = language;
    }

    public void addRun(double real, double user, double sys)
    {
      runs.add(new Run(real,user,sys));
    }

    public void print(PrintWriter out)
    {
      if (!computed)
        compute();
      out.println("    "+language);
      for (Run r : runs)
        r.print(out);
      out.format("      avg %-8.3f %-8.3f %-8.3f\n",avg[0],avg[1],avg[2]);
    }

    public void printRow(PrintWriter out)
    {
      if (!computed)
        compute();
      out.format("%-12s %8d %-8s %8.3f %8.3f %8.3f\n",
                 test.prog.name,test.n,language,avg[0],avg[1],avg[2]);
    }

    public void compute()
    {
      min = new double[]{0.0,0.0,0.0};
      max = new double[]{0.0,0.0,0.0};
      avg = new double[]{0.0,0.0,0.0};
      double[] total = {0.0,0.0,0.0};
      int count = 0;
      for (Run r : runs) {
        for (int i = 0; i < 3; i++) {
          if ((0 == count) || (min[i] > r.values[i]))
            min[i] = r.values[i];
          if ((0 == count) || (max[i] < r.values[i]))
            max[i] = r.values[i];
          total[i] += r.values[i];
        }
        count++;
      }
      for (int i = 0; i < 3; i++)
        avg[i] = total[i]/((double)count);
      computed = true;
    }
  }

  class Run
  {
    double[] values;

    public Run(double real, double user, double sys)
    {
      this.values = new double[3];
      this.values[0] = real;
      this.values[1] = user;
      this.values[2] = sys;
    }

    public void print(PrintWriter out)
    {
      out.format("      %-8.3f %-8.3f %-8.3f\n",values[0],values[1],values[2]);
    }
  }

  Program getProgram(String name)
  {
    Program prog = programs.get(name);
    if (prog == null) {
      prog = new Program(name);
      programs.put(name,prog);
    }
    return prog;
  }

  void process(File firstRun)
    throws IOException
  {
    File[] files = firstRun.listFiles();
    for (File f : files) {
      if (f.isDirectory())
        continue;
      String filename = f.getName();
      String[] parts = filename.split("_");

      if ((4 == parts.length) && (parts[3].equals("time"))) {
        String program = parts[0];
        int n = Integer.parseInt(parts[1]);
        String language = parts[2];

        FileReader fread = new FileReader(f);
        BufferedReader bread = new BufferedReader(fread);
        String line = bread.readLine();
        String[] fields = line.split("\\s+");
        double real = Double.parseDouble(fields[0]);
        double user = Double.parseDouble(fields[1]);
        double sys = Double.parseDouble(fields[2]);
        getProgram(program).getTest(n).getImplementation(language).addRun(real,user,sys);
      }
    }
  }

  public List<String> getCommonLanguages()
  {
    Test[] tests = new Test[programs.size()];
    int pos = 0;
    for (Program p : programs.values()) {
      Test[] pt = p.tests.values().toArray(new Test[]{});
      tests[pos++] = pt[pt.length-1];
    }

    // Get all languages for the first test
    List<String> languages = new ArrayList<String>();
    for (Implementation impl : tests[0].impls.values())
      languages.add(impl.language);

    // Remove any languages that aren't used by all tests
    for (int i = 1; i < tests.length; i++) {
      int lang = 0;
      while (lang < languages.size()) {
        String cur = languages.get(lang);

        if (tests[i] == null)
          System.out.println("tests[i] == null");
        else if (tests[i].impls == null)
          System.out.println("tests[i].impls == null");
        else if (cur == null)
          System.out.println("cur == null");

        if (!tests[i].impls.containsKey(cur))
          languages.remove(lang);
        else
          lang++;
      }
    }

    return languages;
  }

  public void run(String[] args)
    throws IOException
  {
    if (args.length < 2) {
      System.err.println("Usage: ProcessResults <resultsdir> <plotsdir>");
      System.exit(-1);
    }

    String results = args[0];
    String plots = args[1];

    File resultsDir = new File(results);
    File[] files = resultsDir.listFiles();
    for (File f : files) {
      if (f.isDirectory()) {
        System.out.println(f.getName());
        process(f);
      }
    }

    PrintWriter out = new PrintWriter(System.out,true);
    for (Program p : programs.values())
      p.print(out);

    for (Program p : programs.values()) {
      for (Test t : p.tests.values()) {
        for (Implementation impl : t.impls.values()) {
          impl.printRow(out);
        }
      }
    }

    for (Program p : programs.values()) {
      p.dataNvsUser(plots+"/"+p.name);
    }

    programHist(plots+"/hist");
  }

  public static void main(String[] args)
    throws IOException
  {
    ProcessResults pr = new ProcessResults();
    pr.run(args);
  }
}
