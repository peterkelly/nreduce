import java.io.*;
import java.util.*;

public class GenScripts
{
  static class ProgTest
  {
    String name;
    int[] values;
    public ProgTest(String name, int[] values)
    {
      this.name = name;
      this.values = values;
    }
  }

  static void genScript(String scriptsDir, String program, int n, int run)
    throws IOException
  {
    PrintWriter out = new PrintWriter(scriptsDir+"/"+program+"_"+n+"_"+run+".sub");
    out.println("#!/bin/sh");

    out.println("#PBS -V");
    out.println("#PBS -N nreduce");
    out.println("#PBS -j oe");
    out.println("#PBS -q titan");

    out.println("#PBS -l nodes=1:ppn=2,walltime=00:30:00");

    out.println("# This job's working directory");
    out.println("echo Working directory is $PBS_O_WORKDIR");
    out.println("cd $PBS_O_WORKDIR");
    out.println("echo Running on host `hostname`");
    out.println("echo Time is `date`");

    out.println("cd ~/dev/nreduce/benchmark");
    out.println("export ENGINE=native");
    out.println("export PATH=\"~/dev/nreduce/src:$PATH\"");
    out.println("./run.sh "+program+" "+n+" results/run"+run);
    out.close();
  }

  static void genScripts(String scriptsDir, String program, int ns[])
    throws IOException
  {
    for (int n : ns)
      genScript(scriptsDir,program,n,1);
  }

  public static void main(String[] args)
    throws IOException
  {
    if (args.length < 1) {
      System.err.println("Usage: GenScripts <scriptsdir>");
      System.exit(-1);
    }

    String scriptsDir = args[0];

    List<ProgTest> tests = new ArrayList<ProgTest>();
    tests.add(new ProgTest("bintree",new int[]{16,18,20,22}));
    tests.add(new ProgTest("mandelbrot",new int[]{32,64,96,128}));
    tests.add(new ProgTest("matmult",new int[]{100,200,300,400}));
    tests.add(new ProgTest("mergesort",new int[]{100000,200000,300000,400000}));
    tests.add(new ProgTest("quicksort",new int[]{100000,200000,300000,400000}));
    tests.add(new ProgTest("nsieve",new int[]{2000000,4000000,6000000,8000000}));
    tests.add(new ProgTest("nfib",new int[]{30,32,34,36}));

    int maxvalues = 0;
    for (ProgTest test : tests) {
      genScripts(scriptsDir,test.name,test.values);
      if (maxvalues < test.values.length)
        maxvalues = test.values.length;
    }

    PrintWriter out = new PrintWriter(scriptsDir+"/all.sh");
    for (int run = 0; run < 5; run++) {
      for (int i = 0; i < maxvalues; i++) {
        for (ProgTest test : tests) {
          if (i < test.values.length) {
            out.println("./run.sh "+test.name+" "+test.values[i]+" results/run"+run);
          }
        }
      }
    }
    out.close();
  }
}
