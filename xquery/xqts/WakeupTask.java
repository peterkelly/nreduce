import java.util.TimerTask;

class WakeupTask extends TimerTask
{
  private Thread mainThread;
  private boolean done = false;
  public WakeupTask(Thread mainThread)
  {
    this.mainThread = mainThread;
  }

  public boolean cancel()
  {
    boolean r = super.cancel();
    synchronized (this) {
      done = true;
    }
    return true;
  }

  public void run()
  {
    synchronized (this) {
      if (!done)
        mainThread.interrupt();
      done = true;
    }
  }
}
