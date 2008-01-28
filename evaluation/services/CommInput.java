package services;

import java.io.InputStream;
import java.io.ByteArrayInputStream;

public class CommInput
{
  protected byte[] data;
  protected int start;

  public CommInput(byte[] data)
  {
    this.data = data;
    this.start = 0;
  }

  public String readLine()
  {
    int pos = start;
    while ((pos < data.length) && ('\n' != data[pos]))
      pos++;
    String res = new String(data,start,pos-start);
    start = pos+1;
    return res;
  }

  public InputStream readData()
  {
    String lenStr = readLine();
    int len = Integer.parseInt(lenStr);
    ByteArrayInputStream stream = new ByteArrayInputStream(data,start,len);
    start += len;
    return stream;
  }
}
