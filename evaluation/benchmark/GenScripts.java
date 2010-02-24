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

  static void genScript(String scriptsDir, int run)
    throws IOException
  {
    PrintWriter out = new PrintWriter(scriptsDir+"/all"+run+".sub");
    out.println("#!/bin/sh");

    out.println("#PBS -V");
    out.println("#PBS -N benchmark");
    out.println("#PBS -j oe");
    out.println("#PBS -q hydra");

    out.println("#PBS -l nodes=1:ppn=2,walltime=02:00:00");

    out.println("# This job's working directory");
    out.println("echo Working directory is $PBS_O_WORKDIR");
    out.println("cd $PBS_O_WORKDIR");
    out.println("echo Running on host `hostname`");
    out.println("echo Time is `date`");

    out.println("cd ~/dev/evaluation/benchmark");
    out.println("export PATH=\"~/dev/nreduce/src:$PATH\"");
    out.println("scripts/all.sh ~/jobs/benchmark/run"+run);
    out.close();
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
      if (maxvalues < test.values.length)
        maxvalues = test.values.length;
    }

    int numTests = 10;

    for (int i = 0; i < numTests; i++)
      genScript(scriptsDir,i);

    File allFile = new File(scriptsDir+"/all.sh");
    PrintWriter out = new PrintWriter(allFile);

    out.println("#!/bin/bash");
    out.println("OUTDIR=$1");
    out.println("if [ -z \"$OUTDIR\" ]; then");
    out.println("  echo \"Usage: $0 outdir\"");
    out.println("  exit 1");
    out.println("fi");
    out.println();
    out.println("mkdir -p \"$OUTDIR\"");
    out.println();
    for (int i = 0; i < maxvalues; i++) {
      for (ProgTest test : tests) {
        if (i < test.values.length) {
          out.println("./run.sh "+test.name+" "+test.values[i]+" \"$OUTDIR\"");
        }
      }
    }
    out.println();
    out.println("touch \"$OUTDIR/done\"");
    out.close();
    allFile.setExecutable(true);
  }
}
