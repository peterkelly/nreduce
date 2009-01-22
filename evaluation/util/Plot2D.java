/*
 * This file is part of the nreduce project
 * Copyright (C) 2008-2009 Peter Kelly (pmk@cs.adelaide.edu.au)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * $Id$
 *
 */

package util;

import java.io.*;
import java.util.*;
import java.util.regex.*;

public class Plot2D extends Plot
{
  public void genPlot(String name, String title, int col, String ylabel, boolean ideal)
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

    if (ideal) {
      plotOut.print("x title \"Ideal speedup\" with lines ls 0, \\\n");
    }

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
      double t1 = 0.0; // if no n=1, can't compute speedup/efficiency
      try {
        t1 = getUserTime(testdirs[i],maxr,1,0,false).avg;
      } catch (FileNotFoundException e) {}
      PrintWriter dataOut = openData(outdir+"/"+testnames[i]+".dat");
      for (int n : nvalues) {
        Result rn = getUserTime(testdirs[i],maxr,n,0,false);
        printData(dataOut,n,0,rn,t1);
      }
      closeData(dataOut);
    }

    // Generate plots

    genPlot("time","Time",AVG,"Time (s)",false);
    genPlot("speedup","Speedup",SPEEDUP,"Speedup",true);
    genPlot("efficiency","Efficiency",EFFICIENCY,"Efficiency",false);
  }

  public static void main(String[] args)
    throws IOException
  {
    new Plot2D().run(args);
  }
}
