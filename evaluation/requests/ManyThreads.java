public class ManyThreads
{
  class EmptyThread extends Thread
  {
    public void run()
    {
      synchronized (ManyThreads.this) {
      }
    }
  }

  class TestThread extends Thread
  {
    public void run()
    {
    }
  }

  public void timeThreadSpawn()
  {
    int nthreads = 100000;
    int i;
    long start = System.currentTimeMillis();
    for (i = 0; i < nthreads; i++) {
      TestThread t = new TestThread();
      t.start();
      try {
        t.join();
      }
      catch (InterruptedException e) {
        e.printStackTrace();
      }
    }
    long end = System.currentTimeMillis();
    long ms = end-start;
    double rate = 1000.0*((double)nthreads)/((double)ms);
    System.out.println("Creating + destroying "+nthreads+" threads took "+ms+
                       "ms; rate = "+(int)rate+" threads/sec");
  }

  public void testMaxThreads()
  {
    Thread[] threads = new Thread[10000];
    int threadno = 0;
    synchronized (this) {
      while (threadno < threads.length) {
        try {
          EmptyThread t = new EmptyThread();
          t.start();
          threadno++;
        }
        catch (Throwable t) {
          break;
        }
      }
    }
    System.out.println("Max threads: "+threadno);
  }

  public void doTest()
  {
    timeThreadSpawn();
    testMaxThreads();
  }

  public static void main(String[] args)
  {
    new ManyThreads().doTest();
  }
}
