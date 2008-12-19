package services;

public class Calibrate
{
  public static void main(String[] args)
  {
    int iterations = 1;

    if (args.length > 0) {
      iterations = Integer.parseInt(args[0]);
    }

    Compute comp = new Compute(0,0);
    long start = System.nanoTime();
    comp.compute(iterations);
    long end = System.nanoTime();
    System.out.format("Time to execute %d iteration(s) = %.0fms\n",
                      iterations,
                      ((double)(end-start))/1000000.0);
  }
}
