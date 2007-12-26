#!/usr/bin/python
import math
import sys

def mandel(Cr,Ci,iterations):
  Zr = 0
  Zi = 0
  res = 0
  while res < iterations:
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

def mloop(minx,maxx,xincr,miny,maxy,yincr):
  y = miny
  maxx += xincr;
  while y <= maxy:
    x = minx
    while x < maxx:
      printcell(mandel(x,y,4096))
      x = x + xincr
    sys.stdout.write("\n")
    y = y + yincr

incr = 0.01
if (2 <= len(sys.argv)):
  incr = float(sys.argv[1])
print "incr =", incr
mloop(-1.5,0.5,incr,-1.0,1.0,incr);
