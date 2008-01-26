package util;

import java.io.*;
import java.util.*;
import java.util.regex.*;

public class Plot3D extends Plot
{
  public void genPlot(String name, String title, int col, String zlabel)
    throws IOException
  {
    StringWriter sw = new StringWriter();
    PrintWriter plotOut = new PrintWriter(sw);

    plotOut.println("set title \""+title+"\"");
    plotOut.println("set xlabel \"# nodes\"");
    plotOut.println("set ylabel \"Iterations\"");
    plotOut.println("set zlabel \""+zlabel+"\"");

    plotOut.println("set xtics 0,1");
    plotOut.println("set yrange [0:]");
    plotOut.println("set xyplane 0");

    plotOut.println("set hidden3d");
    plotOut.println("set pm3d");
    plotOut.println("set style data lines");

    plotOut.println("splot \\");
    for (int i = 0; i < testdirs.length; i++) {
      plotOut.print("\""+outdir+"/"+testnames[i]+".dat\" using 1:2:"+col+
                    " title \""+titles[i]+"\" ");

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
      System.err.println("Usage: Plot3D <outdir> <testdir1> <testdir2> ...");
      System.exit(-1);
    }

    outdir = args[0];

    getTestsInfo(args);

    // Determine values of runs and n

    SortedSet<Integer> nvalues = new TreeSet<Integer>();
    SortedSet<Integer> vvalues = new TreeSet<Integer>();
    int maxr = determineParams(new File(testdirs[0]),nvalues,vvalues);

    // Create data files

    for (int i = 0; i < ntests; i++) {

      double[] t1 = new double[vvalues.size()];
      int vnum = 0;
      for (int v : vvalues)
        t1[vnum++] = getUserTime(testdirs[i],maxr,1,v,true).avg;

      PrintWriter dataOut = openData(outdir+"/"+testnames[i]+".dat");
      for (Integer nval : nvalues) {
        int n = nval.intValue();
        vnum = 0;
        for (int v : vvalues) {
          Result rn = getUserTime(testdirs[i],maxr,n,v,true);
          printData(dataOut,n,v,rn,t1[vnum]);
          vnum++;
        }
        dataOut.format("\n");
      }
      closeData(dataOut);
    }

    // Generate plots

    genPlot("time","Time",AVG,"Time (s)");
    genPlot("speedup","Speedup",SPEEDUP,"Speedup");
    genPlot("efficiency","Efficiency",EFFICIENCY,"Efficiency");
  }

  public static void main(String[] args)
    throws IOException
  {
    new Plot3D().run(args);
  }
}
