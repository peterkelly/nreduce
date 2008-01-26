package util;

import java.io.*;
import java.util.*;
import java.util.regex.*;

public class Plot2D extends Plot
{
  public void genPlot(String name, String title, int col, String ylabel)
    throws IOException
  {
    StringWriter sw = new StringWriter();
    PrintWriter plotOut = new PrintWriter(sw);

    String options = " with lines linewidth 2";

    plotOut.println("set title \""+title+"\"");
    plotOut.println("set xlabel \"# nodes\"");
    plotOut.println("set ylabel \""+ylabel+"\"");
    plotOut.println("set yrange [0:]");

    plotOut.println("plot \\");

    for (int i = 0; i < testdirs.length; i++) {
      plotOut.print("\""+outdir+"/"+testnames[i]+".dat\" using 1:"+col+
                    " title \""+titles[i]+"\" "+options);
      if (i+1 < testdirs.length)
        plotOut.print(", \\");
      plotOut.println();
    }

    plotOut.close();

    makePlot(name,sw.toString());
  }

  public void run(String[] args)
    throws IOException
  {
    if (args.length < 2) {
      System.err.println("Usage: Plot <outdir> <testdir1> <testdir2> ...");
      System.exit(-1);
    }

    outdir = args[0];

    getTestsInfo(args);

    // Determine values of runs and n

    SortedSet<Integer> nvalues = new TreeSet<Integer>();
    int maxr = determineParams(new File(testdirs[0]),nvalues,null);

    // Create data files

    for (int i = 0; i < ntests; i++) {
      double t1 = getUserTime(testdirs[i],maxr,1,0,false).avg;

      PrintWriter dataOut = new PrintWriter(outdir+"/"+testnames[i]+".dat");
      dataOut.format("%8s %8s %10s %10s %10s %10s %10s %10s\n",
                     "#n","v","min","max","avg","ideal","speedup","efficiency");
      for (Integer nval : nvalues) {
        int n = nval.intValue();

        Result rn = getUserTime(testdirs[i],maxr,n,0,false);
        double tn = rn.avg;
        double ideal = t1/n;
        double speedup = t1/tn;
        double efficiency = speedup/n;
        dataOut.format("%8d %8d %10.3f %10.3f %10.3f %10.3f %10.3f %10.3f\n",
                       n,0,rn.min,rn.max,rn.avg,ideal,speedup,efficiency);
      }
      dataOut.close();
    }

    // Generate plots

    genPlot("time","Time",AVG,"Time (s)");
    genPlot("speedup","Speedup",SPEEDUP,"Speedup");
    genPlot("efficiency","Efficiency",EFFICIENCY,"Efficiency");
  }

  public static void main(String[] args)
    throws IOException
  {
    new Plot2D().run(args);
  }
}
