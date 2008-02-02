package services;

import java.io.IOException;
import java.io.InputStream;
import java.io.ByteArrayInputStream;

public class CommInput
{
  protected InputStream in;

  public CommInput(InputStream in)
  {
    this.in = in;
  }

  public String readLine()
    throws IOException
  {
    StringBuffer buf = new StringBuffer();
    while (true) {
      int c = in.read();
      if ((-1 == c) || ('\n' == c))
        break;
      buf.append((char)c);
    }
    return buf.toString();
  }

  public InputStream readData()
    throws IOException
  {
    String lenStr = readLine();
    int len = Integer.parseInt(lenStr);
    byte[] data = new byte[len];
    int pos = 0;
    while (pos < len) {
      int r = in.read(data,pos,len-pos);
      if (0 > r)
        throw new IOException("Unexpected end of stream");
      pos += r;
    }
    return new ByteArrayInputStream(data,0,len);
  }
}
