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

public class Calibrate
{
  public static void main(String[] args)
  {
    int iterations = 1;

    if (args.length > 0) {
      iterations = Integer.parseInt(args[0]);
    }

    Compute comp = new Compute(0,0);
    long start = System.nanoTime();
    comp.compute(iterations);
    long end = System.nanoTime();
    System.out.format("Time to execute %d iteration(s) = %.0fms\n",
                      iterations,
                      ((double)(end-start))/1000000.0);
  }
}
