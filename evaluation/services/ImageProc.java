package services;

import java.net.*;
import java.io.*;
import java.awt.image.*;
import javax.imageio.*;

public class ImageProc
{
  static void handle(Socket c, int id) throws Exception
  {
    try {
      InputStream cin = c.getInputStream();
      OutputStream cout = c.getOutputStream();
      PrintWriter writer = new PrintWriter(cout);
      InputStreamReader reader = new InputStreamReader(cin);

      String line = new BufferedReader(reader).readLine();
      String[] args = line.split("\\s+");
      writer.println("nargs = "+args.length);
      for (int i = 0; i < args.length; i++)
        writer.println("args["+i+"] = \""+args[i]+"\"");
      

//       System.out.println("line = \""+line+"\"");
//       char[] chars = line.toCharArray();
//       for (int i = 0; i < chars.length; i++)
//         chars[i]++;
//       String result = new String(chars);
//       writer.print(result);
      writer.flush();
    }
    finally {
      c.close();
      System.out.println(id+": connection closed");
    }
  }

  static void smooth(int width, int height, int[] pixels, int n)
  {
    long startTime = System.currentTimeMillis();
    int[] result = new int[pixels.length];
    for (int i = 0; i < n; i++) {
      for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
          for (int col = 0; col < 3; col++) {
            int total = 0;
            int count = 0;
            if (x > 0) { // left
              total += pixels[3*(y*width+(x-1))+col];
              count++;
            }
            if (x < width-1) { // right
              total += pixels[3*(y*width+(x+1))+col];
              count++;
            }
            if (y > 0) { // up
              total += pixels[3*((y-1)*width+x)+col];
              count++;
            }
            if (y < height-1) { // down
              total += pixels[3*((y+1)*width+x)+col];
              count++;
            }
            if ((x > 0) && (y > 0)) { // up-left
              total += pixels[3*((y-1)*width+(x-1))+col];
              count++;
            }
            if ((x < width-1) && (y > 0)) { // up-right
              total += pixels[3*((y-1)*width+(x+1))+col];
              count++;
            }
            if ((x > 0) && (y < height-1)) { // down-left
              total += pixels[3*((y+1)*width+(x-1))+col];
              count++;
            }
            if ((x < width-1) && (y < height-1)) { // down-right
              total += pixels[3*((y+1)*width+(x+1))+col];
              count++;
            }
            result[3*(y*width+x)+col] = (int)(((double)total)/((double)count));
          }
        }
      }
      System.arraycopy(result,0,pixels,0,pixels.length);
    }
    long endTime = System.currentTimeMillis();
    System.out.println(n+" smoothing iterations took "+(endTime-startTime)+" ms");
  }

  static void standalone(String infilename, String outfilename)
    throws IOException
  {
    BufferedImage img = ImageIO.read(new File(infilename));
    WritableRaster w = img.getRaster();
    int width = w.getWidth();
    int height = w.getHeight();
    System.out.println("width = "+width+", height = "+height);
    int[] pixels = w.getPixels(0,0,w.getWidth(),w.getHeight(),(int[])null);
    System.out.println("pixels.length = "+pixels.length);
    smooth(width,height,pixels,10);
    w.setPixels(0,0,width,height,pixels);
    BufferedImage mod = new BufferedImage(img.getColorModel(),w,false,null);
    ImageIO.write(mod,"jpg",new File(outfilename));
  }

  static void server(int port)
    throws IOException
  {
    byte[] b = new byte[]{0,0,0,0};
    InetAddress addr = InetAddress.getByAddress(b);
    ServerSocket s = new ServerSocket(port,100,addr);
    System.out.println("Started server socket on port "+port);
    for (int nextid = 0; true; nextid++) {
      final Socket c = s.accept();
      final int id = nextid;
      System.out.println(id+": accepted connection");
      new Thread() { public void run() {
        try { handle(c,id); } catch (Exception e) { }
      } }.start();
    }
  }

  public static void main(String[] args) throws Exception
  {
    System.out.println("Input formats:");
    for (String s : ImageIO.getReaderFormatNames())
      System.out.println("  "+s);

    System.out.println("Output formats:");
    for (String s : ImageIO.getWriterFormatNames())
      System.out.println("  "+s);


    if ((3 == args.length) && (args[0].equals("standalone"))) {
      standalone(args[1],args[2]);
    }
    else if (1 == args.length) {
      server(Integer.parseInt(args[0]));
    }
    else {
      System.out.println("Usage: "+ImageProc.class.getName()+" <port>");
      System.out.println("or     "+ImageProc.class.getName()+" standalone <input> <output>");
      System.exit(-1);
    }
  }
}
