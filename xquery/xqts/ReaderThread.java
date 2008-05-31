import java.io.InputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

public class ReaderThread extends Thread
{
  private InputStream in;
  private ByteArrayOutputStream out = new ByteArrayOutputStream();
  private boolean done = false;
  private IOException exception = null;

  public ReaderThread(InputStream in)
  {
    this.in = in;
  }

  public void run()
  {
    try {
      byte[] buf = new byte[1024];
      int r;
      while (0 <= (r = in.read(buf))) {
        out.write(buf,0,r);
      }
      in.close();
      out.close();
      synchronized (this) {
        done = true;
        notifyAll();
      }
    }
    catch (IOException e) {
      synchronized (this) {
        done = true;
        exception = e;
        notifyAll();
      }
    }
  }

  public byte[] getData()
    throws IOException
  {
    synchronized (this) {
      while (!done) {
        try {
          wait();
        }
        catch (InterruptedException e) {}
      }
      if (exception != null)
        throw exception;
      return out.toByteArray();
    }
  }
}
