import java.net.Socket;
import java.net.InetSocketAddress;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class Requests
{
  String host;
  int port;
  String path;

  int total = 0;
  int remaining = 0;
  int active = 0;
  int maxactive = 0;
  int nthreads = 0;
  int maxthreads = 0;

  final static boolean DEBUG_THREADS = true;
//   final static int CONNECTION_LIMIT = 100;
  final static int CONNECTION_LIMIT = 100000;

  public Requests(String host, int port, String path)
  {
    this.host = host;
    this.port = port;
    this.path = path;
  }

  public void singleRequest()
    throws IOException
  {
    synchronized (this) {
      nthreads++;
      if (maxthreads < nthreads) {
//         System.out.println("maxthreads = "+maxthreads);
        maxthreads = nthreads;
      }

      while (active >= CONNECTION_LIMIT) {
        try {
          this.wait();
        }
        catch (InterruptedException e) {}
      }

      active++;
      if (maxactive < active)
        maxactive = active;
      if (DEBUG_THREADS)
        System.out.println("active = "+active);
    }
    Socket sock = new Socket();
    sock.connect(new InetSocketAddress(host,port));

    InputStream in = sock.getInputStream();
    OutputStream out = sock.getOutputStream();

    out.write(("GET "+path+" HTTP/1.0\r\n\r\n").getBytes());
    byte[] buf = new byte[1024];
    int r;
    int bytesread = 0;
    while (0 < (r = in.read(buf)))
      bytesread += r;
    in.close();
    out.close();
    sock.close();

    if (DEBUG_THREADS)
      System.out.println("Bytes read: "+bytesread);

    synchronized (this) {
      total += bytesread;
      remaining--;
      this.notifyAll();
    }
    synchronized (this) {
      active--;
      nthreads--;
      if (DEBUG_THREADS)
        System.out.println("active = "+active);
    }
  }

  public void doRequests(int n)
  {
    remaining = n;
    for (int i = 0; i < n; i++) {
      final int threadno = i;
      Thread t = new Thread() {
          public void run() {
            try {
              singleRequest();
//               System.out.println("Thread "+threadno+" finished");
            }
            catch (IOException e) {
//               e.printStackTrace();
//               System.exit(-1);
            }
          }
        };
      t.start();
//       System.out.println("Started thread "+i);
    }

    synchronized (this) {
      while (0 < remaining) {
        try {
          this.wait();
        }
        catch (InterruptedException e) {}
      }
    }

    System.out.println("Total: "+total+", maxactive = "+maxactive+", maxthreads = "+maxthreads);
  }


  public static void main(String[] args)
    throws Exception
  {
    if (args.length < 3) {
      System.err.println("Usage: Requests <host> <port> <n>");
      System.exit(-1);
    }

    int n = Integer.parseInt(args[0]);
    String host = args[1];
    int port = Integer.parseInt(args[2]);
    String path;
    if (args.length >= 4)
      path = args[3];
    else
      path = "/";

    System.out.println("Connection limit = "+CONNECTION_LIMIT);
    Requests r = new Requests(host,port,path);
    r.doRequests(n);
  }
}
