package services;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.File;
import java.awt.Point;
import java.awt.Color;
import java.awt.image.BufferedImage;
import java.awt.image.Raster;
import java.awt.image.WritableRaster;
import java.awt.image.DataBufferByte;
import java.awt.image.ComponentSampleModel;
import javax.imageio.ImageIO;
import javax.imageio.ImageWriter;
import javax.imageio.ImageWriteParam;
import javax.imageio.IIOImage;
import javax.imageio.stream.MemoryCacheImageOutputStream;
import javax.imageio.stream.MemoryCacheImageOutputStream;

public class ImageProc extends Server
{
  private String imageDir;

  public ImageProc(int maxThreads, int port, String imageDir)
  {
    super(maxThreads,port);
    this.imageDir = imageDir;
  }

  static void writeJpeg(BufferedImage img, OutputStream out)
    throws IOException
  {
    ImageWriter writer = ImageIO.getImageWritersByFormatName("jpg").next();
    ImageWriteParam iwp = writer.getDefaultWriteParam();
    iwp.setCompressionMode(ImageWriteParam.MODE_EXPLICIT);
    iwp.setCompressionQuality((float)0.95);
    writer.setOutput(new MemoryCacheImageOutputStream(out));
    writer.write(null,new IIOImage(img,null,null),iwp);
  }

  static WritableRaster createRaster(int width, int height, byte[] pixels, BufferedImage base)
  {
    ComponentSampleModel model = (ComponentSampleModel)base.getRaster().getSampleModel();
    WritableRaster rw =
      Raster.createInterleavedRaster(new DataBufferByte(pixels,pixels.length),
                                     width,height,width*3,
                                     model.getPixelStride(),model.getBandOffsets(),new Point(0,0));
    return rw;
  }

  static void analyze(InputStream in, OutputStream out)
    throws IOException
  {
    BufferedImage img = ImageIO.read(in);
    WritableRaster w = img.getRaster();
    int width = w.getWidth();
    int height = w.getHeight();
    byte[] pixels = ((DataBufferByte)w.getDataBuffer()).getData();

    double totalHue = 0.0; // hue
    double totalSaturation = 0.0; // saturation
    double totalValue = 0.0; // luminance
    double totalWeightedHue = 0.0; // weighted hue
    double totalWeight = 0.0;

    float[] hsv = new float[3];
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {

        int red = (256+(int)pixels[3*(y*width+x)+2])%256;
        int green = (256+(int)pixels[3*(y*width+x)+1])%256;
        int blue = (256+(int)pixels[3*(y*width+x)+0])%256;

        Color.RGBtoHSB(red,green,blue,hsv);

        totalHue += (double)hsv[0];
        totalSaturation += (double)hsv[1];
        totalValue += (double)hsv[2];
        // give more weight to pixels that are bright and saturated
        totalWeightedHue += (double)hsv[0]*hsv[1]*hsv[2];
        totalWeight += hsv[1]*hsv[2];
      }
    }

    double hue = totalHue/(double)(width*height);
    double saturation = totalSaturation/(double)(width*height);
    double value = totalValue/(double)(width*height);
    double weightedHue = (totalWeight == 0.0) ? 0.0 : totalWeightedHue/totalWeight;

    PrintWriter writer = new PrintWriter(out,true);
    writer.format("%d %d %.3f %.3f %.3f %.3f\n",
                  width,height,hue,saturation,value,weightedHue);
  }

  static void smooth(InputStream in, int iterations, OutputStream out)
    throws IOException
  {
    BufferedImage img = ImageIO.read(in);
    WritableRaster w = img.getRaster();
    int width = w.getWidth();
    int height = w.getHeight();
    byte[] pixels = ((DataBufferByte)w.getDataBuffer()).getData();

    byte[] result = new byte[pixels.length];
    for (int i = 0; i < iterations; i++) {
      for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
          for (int col = 0; col < 3; col++) {
            int total = 0;
            int count = 0;
            if (x > 0) { // left
              total += (256+(int)pixels[3*(y*width+(x-1))+col])%256;
              count++;
            }
            if (x < width-1) { // right
              total += (256+(int)pixels[3*(y*width+(x+1))+col])%256;
              count++;
            }
            if (y > 0) { // up
              total += (256+(int)pixels[3*((y-1)*width+x)+col])%256;
              count++;
            }
            if (y < height-1) { // down
              total += (256+(int)pixels[3*((y+1)*width+x)+col])%256;
              count++;
            }
            if ((x > 0) && (y > 0)) { // up-left
              total += (256+(int)pixels[3*((y-1)*width+(x-1))+col])%256;
              count++;
            }
            if ((x < width-1) && (y > 0)) { // up-right
              total += (256+(int)pixels[3*((y-1)*width+(x+1))+col])%256;
              count++;
            }
            if ((x > 0) && (y < height-1)) { // down-left
              total += (256+(int)pixels[3*((y+1)*width+(x-1))+col])%256;
              count++;
            }
            if ((x < width-1) && (y < height-1)) { // down-right
              total += (256+(int)pixels[3*((y+1)*width+(x+1))+col])%256;
              count++;
            }
            result[3*(y*width+x)+col] = (byte)(((double)total)/((double)count));
          }
        }
      }
      System.arraycopy(result,0,pixels,0,pixels.length);
    }

    writeJpeg(img,out);
  }

  static void resize(InputStream in, int outWidth, int outHeight, OutputStream out)
    throws IOException
  {
    BufferedImage img = ImageIO.read(in);
    WritableRaster w = img.getRaster();
    int inWidth = w.getWidth();
    int inHeight = w.getHeight();
    byte[] data = ((DataBufferByte)w.getDataBuffer()).getData();

    long startTime = System.currentTimeMillis();
    byte[] resized = new byte[outWidth*outHeight*3];

    if ((outHeight < inHeight) && (outWidth < inWidth)) {
      // Simple resize
      for (int y = 0; y < outHeight; y++) {
        int srcy1 = y*inHeight/outHeight;
        int srcy2 = (y+1)*inHeight/outHeight;
        for (int x = 0; x < outWidth; x++) {
          int srcx1 = x*inWidth/outWidth;
          int srcx2 = (x+1)*inWidth/outWidth;

          for (int col = 0; col < 3; col++) {
            int total = 0;
            int count = 0;
            for (int ay = srcy1; ay < srcy2; ay++) {
              for (int ax = srcx1; ax < srcx2; ax++) {
                total += (256+(int)data[3*(ay*inWidth+ax)+col])%256;
                count++;
              }
            }
            resized[3*(y*outWidth+x)+col] = (byte)(((double)total)/((double)count));
          }
        }
      }
    }
    else {
      // Interpolate
      double heightRatio = ((double)inHeight)/((double)outHeight);
      double widthRatio = ((double)inWidth)/((double)outWidth);
      for (int y = 0; y < outHeight; y++) {
        double miny = ((double)y)*heightRatio;
        double maxy = ((double)(y+1))*heightRatio;
        for (int x = 0; x < outWidth; x++) {
          double minx = ((double)x)*widthRatio;
          double maxx = ((double)(x+1))*widthRatio;

          for (int col = 0; col < 3; col++) {
            int total = 0;
            int count = 0;
            for (double ay = miny; ay < maxy; ay++) {
              for (double ax = minx; ax < maxx; ax++) {
                double leftx = Math.floor(ax);
                double rightx = (leftx+1 <= inWidth-1) ? leftx+1 : inWidth-1;
                double topy = Math.floor(ay);
                double bottomy = (topy+1 <= inHeight-1) ? topy+1 : inHeight-1;

                int topleft = (256+(int)data[3*(((int)topy)*inWidth+(int)leftx)+col])%256;
                int topright = (256+(int)data[3*(((int)topy)*inWidth+(int)rightx)+col])%256;
                int bottomleft = (256+(int)data[3*(((int)bottomy)*inWidth+(int)leftx)+col])%256;
                int bottomright = (256+(int)data[3*(((int)bottomy)*inWidth+(int)rightx)+col])%256;

                total += (int)
                  ((((rightx-ax)+(bottomy-ay))*topleft+
                    (ax-leftx+(bottomy-ay))*topright+
                    ((rightx-ax)+ay-topy)*bottomleft+
                    (ax-leftx+ay-topy)*bottomright)/4);
                count++;
              }
            }
            resized[3*(y*outWidth+x)+col] = (byte)(((double)total)/((double)count));
          }
        }
      }
    }

    long endTime = System.currentTimeMillis();

    WritableRaster rw = createRaster(outWidth,outHeight,resized,img);
    BufferedImage mod = new BufferedImage(img.getColorModel(),rw,false,null);
    writeJpeg(mod,out);
  }

  static void combine(OutputStream out, InputStream[] inputs)
    throws IOException
  {

    int size = (int)Math.ceil(Math.sqrt(inputs.length));

    // Read image
    BufferedImage[][] grid = new BufferedImage[size][size];
    int x = 0;
    int y = 0;
    for (int i = 0; i < inputs.length; i++) {
      grid[y][x] = ImageIO.read(inputs[i]);
      x++;
      if (x >= size) {
        y++;
        x = 0;
      }
    }

    // Determine row/column sizes
    int[] colWidths = new int[size];
    int[] rowHeights = new int[size];
    for (y = 0; y < size; y++) {
      for (x = 0; x < size; x++) {
        if ((null != grid[y][x]) && (rowHeights[y] < grid[y][x].getHeight()))
          rowHeights[y] = grid[y][x].getHeight();
        if ((null != grid[y][x]) && (colWidths[x] < grid[y][x].getWidth()))
          colWidths[x] = grid[y][x].getWidth();
      }
    }

    // Determine total output size
    int outWidth = 0;
    int outHeight = 0;
    for (x = 0; x < size; x++)
      outWidth += colWidths[x];
    for (y = 0; y < size; y++)
      outHeight += rowHeights[y];

    // Draw images on grid
    byte[] pixels = new byte[outWidth*outHeight*3];
    x = 0;
    y = 0;
    int px = 0;
    int py = 0;
    for (int i = 0; i < inputs.length; i++) {
      BufferedImage img = grid[y][x];
      WritableRaster w = img.getRaster();
      int iwidth = w.getWidth();
      int iheight = w.getHeight();
      byte[] data = ((DataBufferByte)w.getDataBuffer()).getData();

      for (int iy = 0; iy < iheight; iy++) {
        for (int ix = 0; ix < iwidth; ix++) {
          pixels[3*((py+iy)*outWidth+px+ix)+0] = data[3*(iy*iwidth+ix)+0];
          pixels[3*((py+iy)*outWidth+px+ix)+1] = data[3*(iy*iwidth+ix)+1];
          pixels[3*((py+iy)*outWidth+px+ix)+2] = data[3*(iy*iwidth+ix)+2];
        }
      }

      px += colWidths[x];
      x++;
      if (x >= size) {
        py += rowHeights[y];
        y++;
        px = 0;
        x = 0;
      }
    }

    WritableRaster rw = createRaster(outWidth,outHeight,pixels,grid[0][0]);
    BufferedImage mod = new BufferedImage(grid[0][0].getColorModel(),rw,false,null);
    writeJpeg(mod,out);
  }

  public void process(InputStream cin, OutputStream cout) throws Exception
  {
    long startTime = System.currentTimeMillis();
    CommInput input = new CommInput(cin);
    String line = input.readLine();
    String[] args = line.trim().split("\\s+");

    cout.write(65);

    if ((1 == args.length) && (args[0].equals("list"))) {
      File dir = new File(imageDir);
      File[] contents = dir.listFiles();
      PrintWriter writer = new PrintWriter(cout);
      for (File f : contents) {
        if (f.getName().toLowerCase().endsWith(".jpg"))
          writer.println(f.getName());
      }
      writer.flush();
    }
    else if ((2 == args.length) && (args[0].equals("get"))) {
      String path = imageDir+"/"+args[1];
      FileInputStream fin = new FileInputStream(path);
      int r;
      byte[] buf = new byte[1024];
      while (0 <= (r = fin.read(buf)))
        cout.write(buf,0,r);
      fin.close();
    }
    else if ((1 == args.length) && (args[0].equals("analyze"))) {
      InputStream in = input.readData();
      analyze(in,cout);
      in.close();
    }
    else if ((2 == args.length) && (args[0].equals("smooth"))) {
      int iterations = Integer.parseInt(args[1]);
      InputStream in = input.readData();
      smooth(in,iterations,cout);
      in.close();
    }
    else if ((3 == args.length) && (args[0].equals("resize"))) {
      int outWidth = Integer.parseInt(args[1]);
      int outHeight = Integer.parseInt(args[2]);
      InputStream in = input.readData();
      resize(in,outWidth,outHeight,cout);
      in.close();
    }
    else if ((2 == args.length) && (args[0].equals("combine"))) {
      int nimages = Integer.parseInt(args[1]);
      InputStream[] inputs = new InputStream[nimages];
      for (int i = 0; i < nimages; i++)
        inputs[i] = input.readData();

      combine(cout,inputs);
      for (InputStream s : inputs)
        s.close();
    }
    else {
      PrintWriter writer = new PrintWriter(cout);
      writer.println("Invalid request: "+line);
      writer.flush();
    }
    long endTime = System.currentTimeMillis();
    System.out.format("%-60s %d\n",line,(endTime-startTime));
  }

  public static void main(String[] args) throws Exception
  {
    if ((2 == args.length) && (args[0].equals("analyze"))) {
      FileInputStream in = new FileInputStream(args[1]);
      analyze(in,System.out);
      in.close();
    }
    else if ((4 == args.length) && (args[0].equals("smooth"))) {
      FileInputStream in = new FileInputStream(args[1]);
      int iterations = Integer.parseInt(args[2]);
      FileOutputStream out = new FileOutputStream(args[3]);
      smooth(in,iterations,out);
      in.close();
      out.close();
    }
    else if ((5 == args.length) && (args[0].equals("resize"))) {
      FileInputStream in = new FileInputStream(args[1]);
      int outWidth = Integer.parseInt(args[2]);
      int outHeight = Integer.parseInt(args[3]);
      FileOutputStream out = new FileOutputStream(args[4]);
      resize(in,outWidth,outHeight,out);
      in.close();
      out.close();
    }
    else if ((3 <= args.length) && (args[0].equals("combine"))) {
      String outfilename = args[1];
      InputStream[] inputs = new InputStream[args.length-2];
      for (int i = 0; i < inputs.length; i++)
        inputs[i] = new FileInputStream(args[i+2]);
      OutputStream out = new FileOutputStream(outfilename);
      combine(out,inputs);
      for (InputStream s : inputs)
        s.close();
    }
    else if (2 == args.length) {
      int port = Integer.parseInt(args[0]);
      String imageDir = args[1];
      int maxThreads = 3;
      ImageProc ip = new ImageProc(maxThreads,port,imageDir);
      ip.serve();
    }
    else {
      System.out.println("Usage: services.ImageProc <port>");
      System.out.println("or     services.ImageProc [command]");
      System.out.println("where [command] is done of:");
      System.out.println("  analyze <input>");
      System.out.println("  smooth <input> <iterations> <output>");
      System.out.println("  resize <input> <width> <height> <output>");
      System.out.println("  combine <output> <inputs...>");
      System.exit(-1);
    }
  }
}
