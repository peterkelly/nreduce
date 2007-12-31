#!/usr/bin/python
import math
import sys

def mandel(Cr,Ci):
  Zr = 0.0
  Zi = 0.0
  res = 0
  while res < 4096:
    newZr = (Zr*Zr) - (Zi*Zi) + Cr
    newZi = (2.0*(Zr*Zi)) + Ci
    mag = math.sqrt((newZr*newZr)+(newZi*newZi))
    if (mag > 2.0):
      return res

    Zr = newZr
    Zi = newZi
    res = res + 1

  return res

def printcell(num):
  if (num > 40):
    sys.stdout.write("  ")
  elif (num > 30):
    sys.stdout.write("''")
  elif (num > 20):
    sys.stdout.write("--")
  elif (num > 10):
    sys.stdout.write("**")
  else:
    sys.stdout.write("##")

def mloop(minx,maxx,miny,maxy,incr):
  y = miny
  while y < maxy:

    x = minx
    while x < maxx:
      printcell(mandel(x,y))
      x = x+incr

    sys.stdout.write("\n")
    y = y+incr

n = 32
if (len(sys.argv) >= 2):
  n = int(sys.argv[1])

incr = 2.0/n
mloop(-1.5,0.5,-1.0,1.0,incr);
