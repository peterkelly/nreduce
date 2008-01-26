package util;

import java.io.File;
import java.io.IOException;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.PrintWriter;
import java.util.SortedSet;
import java.util.regex.Pattern;
import java.util.regex.Matcher;

public abstract class Plot
{
  static final int MIN        = 3;
  static final int MAX        = 4;
  static final int AVG        = 5;
  static final int IDEAL      = 6;
  static final int SPEEDUP    = 7;
  static final int EFFICIENCY = 8;

  protected String outdir;
  protected int ntests;
  protected String[] testdirs;
  protected String[] titles;
  protected String[] testnames;

  protected void runGnuplot(String plotfile)
    throws IOException
  {
    String[] cmds = {"gnuplot",plotfile};
    Process proc = Runtime.getRuntime().exec(cmds);
    try {
      int rc = proc.waitFor();
      if (0 != rc)
        System.out.println("gnuplot "+plotfile+" exited with return code "+rc);
    }
    catch (InterruptedException ex) {
      System.out.println("Interrupted");
    }
  }

  protected void makePlot(String name, String plotCommands)
    throws IOException
  {
    String x11PlotFilename = outdir+"/"+name+"-x11.plot";
    String epsPlotFilename = outdir+"/"+name+"-eps.plot";

    PrintWriter w = new PrintWriter(x11PlotFilename);
    w.print(plotCommands);
    w.close();

    w = new PrintWriter(epsPlotFilename);
    w.println("set terminal postscript eps color");
    w.println("set out \""+outdir+"/"+name+".eps\"");
    w.print(plotCommands);
    w.close();

    runGnuplot(epsPlotFilename);
  }

  public static Result getUserTime(String dirName, int maxr, int n, int v, boolean usev)
    throws IOException
  {
    String suffix = usev ? ".n"+n+".v"+v : ".n"+n;
    double min = 0.0;
    double max = 0.0;
    double total = 0.0;
    double count = 0.0;
    for (int r = 0; r <= maxr; r++) {
      String filename = dirName+"/time.r"+r+suffix;
      BufferedReader in = new BufferedReader(new FileReader(filename));
      String line = in.readLine();
      String[] fields = line.split("\\s+");
      double time = Double.parseDouble(fields[0]);

      if ((0 == r) || (min > time))
        min = time;
      if ((0 == r) || (max < time))
        max = time;

      total += time;
      count++;
      in.close();
    }
    double avg = total/count;
    return new Result(min,max,avg);
  }

  protected int determineParams(File dir, SortedSet<Integer> nvalues, SortedSet<Integer> vvalues)
  {
    int maxr = 0;
    boolean first = true;
    File[] contents = dir.listFiles();
    Pattern p;

    if (null != vvalues)
      p = Pattern.compile("time\\.r(\\d+).n(\\d+).v(\\d+)");
    else
      p = Pattern.compile("time\\.r(\\d+).n(\\d+)");

    for (File f : contents) {
      Matcher m = p.matcher(f.getName());
      if (m.matches()) {
        int r = Integer.parseInt(m.group(1));
        int n = Integer.parseInt(m.group(2));
        nvalues.add(n);

        if (null != vvalues) {
          int v = Integer.parseInt(m.group(3));
          vvalues.add(v);
        }

        if (first || (maxr < r))
          maxr = r;
        first = false;
      }
    }
    return maxr;
  }

  protected void getTestsInfo(String[] args)
    throws IOException
  {
    ntests = args.length-1;
    testdirs = new String[ntests];
    titles = new String[ntests];
    testnames = new String[ntests];

    for (int i = 0; i < testdirs.length; i++) {
      testdirs[i] = args[i+1];
      testnames[i] = new File(testdirs[i]).getName();

      File titleFile = new File(testdirs[i]+"/title");
      if (titleFile.exists()) {
        BufferedReader in = new BufferedReader(new FileReader(titleFile));
        titles[i] = in.readLine();
        in.close();
      }
      else {
        titles[i] = testdirs[i];
      }
    }
  }

}
