package services;

import java.net.Socket;
import java.net.ServerSocket;
import java.net.InetAddress;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;

public abstract class Server
{
  private int maxThreads;
  private int port;
  private int nthreads = 0;

  public Server(int maxThreads, int port)
  {
    this.maxThreads = maxThreads;
    this.port = port;
  }

  public abstract void process(InputStream cin, OutputStream cout) throws Exception;

  public void handle(Socket c, int id)
  {
    try {
      InputStream cin = c.getInputStream();
      OutputStream cout = c.getOutputStream();

      process(cin,cout);

      /* Make sure we read all remaining data from the client before closing the connection.
         Otherwise if there is outstanding data or a FIN sent by the client, closing the socket
         on our end will result in a RST being sent to the client, aborting the connection and
         potentially causing the client to miss some of the data we've sent back.
      See http://java.sun.com/javase/6/docs/technotes/guides/net/articles/connection_release.html */
      byte[] buf = new byte[1024];
      while (0 < cin.read(buf)) {
        // repeat
      }
    }
    catch (Exception e) {
      e.printStackTrace();
    }
    finally {
      try {
        c.close();
      }
      catch (IOException e) {
        e.printStackTrace();
      }
    }
  }

  private int getNthreads()
  {
    synchronized (this) { 
      return nthreads;
    }
  }

  public void serve()
    throws IOException
  {
    byte[] b = new byte[]{0,0,0,0};
    InetAddress addr = InetAddress.getByAddress(b);
    ServerSocket s = new ServerSocket(port,50,addr);
    System.out.println("Started server socket on port "+port);
    for (int nextid = 0; true; nextid++) {

      synchronized (Server.this) {
        while (nthreads+1 > maxThreads) {
          try {
            System.out.println("nthreads = "+getNthreads()+", waiting");
            Server.this.wait();
          }
          catch (InterruptedException e) {}
        }
        nthreads++;
      }

      final Socket c = s.accept();
      final int id = nextid;

//       System.out.println(id+": connection opened: nthreads = "+getNthreads());

      new Thread() { public void run() {
        try {
          handle(c,id);
          synchronized (Server.this) {
            nthreads--;
            Server.this.notifyAll();
          }
//           System.out.println(id+": connection closed: nthreads = "+getNthreads());
        } catch (Exception e) { }
      } }.start();
    }
  }


}
