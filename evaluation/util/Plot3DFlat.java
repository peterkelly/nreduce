package util;

import java.io.*;
import java.util.*;
import java.util.regex.*;

public class Plot3DFlat extends Plot
{
  public void genPlot(String name, String title, int col, String ylabel,
                      SortedSet<Integer> vvalues)
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

    int vnum = 0;
    for (Integer vval : vvalues) {
      int v = vval.intValue();
      plotOut.print("\""+outdir+"/"+testnames[0]+".v"+v+".dat\" using 1:"+col+
                    " title \"v = "+v+"\" "+options);
      if (vnum+1 < vvalues.size())
        plotOut.print(", \\");
      plotOut.println();
      vnum++;
    }

    plotOut.close();

    makePlot(name,sw.toString());
  }

  public void run(String[] args)
    throws IOException
  {
    if (args.length < 2) {
      System.err.println("Usage: Plot3DFlat <outdir> <testdir1> <testdir2> ...");
      System.exit(-1);
    }

    outdir = args[0];

    getTestsInfo(args);

    // Determine values of runs and n

    SortedSet<Integer> nvalues = new TreeSet<Integer>();
    SortedSet<Integer> vvalues = new TreeSet<Integer>();
    int maxr = determineParams(new File(testdirs[0]),nvalues,vvalues);

    if (1 != ntests) {
      System.err.println("This program only supports a single test directory");
      System.exit(-1);
    }

    // Create data files

    for (int v : vvalues) {
      for (int i = 0; i < ntests; i++) {
        double t1 = getUserTime(testdirs[0],maxr,1,v,true).avg;
        PrintWriter dataOut = openData(outdir+"/"+testnames[0]+".v"+v+".dat");
        for (int n : nvalues) {
          Result rn = getUserTime(testdirs[0],maxr,n,v,true);
          printData(dataOut,n,v,rn,t1);
        }
        closeData(dataOut);
      }
    }

    // Generate plots

    genPlot("time","Time",AVG,"Time (s)",vvalues);
    genPlot("speedup","Speedup",SPEEDUP,"Speedup",vvalues);
    genPlot("efficiency","Efficiency",EFFICIENCY,"Efficiency",vvalues);
  }

  public static void main(String[] args)
    throws IOException
  {
    new Plot3DFlat().run(args);
  }
}
