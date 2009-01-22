/*
 * This file is part of the nreduce project
 * Copyright (C) 2008-2009 Peter Kelly (pmk@cs.adelaide.edu.au)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * $Id$
 *
 */

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
