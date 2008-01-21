#!/usr/bin/python

import sys

def genlist(max):
  res = []
  for i in range(0,max):
    rev = str(7*i)[::-1]
    res.append(rev)
  return res

def mergesort(m):
  if len(m) <= 1:
    return m

  middle = len(m)/2
  left = m[0:middle]
  right = m[middle:]

  left = mergesort(left)
  right = mergesort(right)

  return merge(left,right)

def merge(xlst,ylst):
  x = 0
  y = 0
  res = []
  while (x < len(xlst)) and (y < len(ylst)):
    xval = xlst[x]
    yval = ylst[y]
    if xval < yval:
      res.append(xval)
      x = x+1
    else:
      res.append(yval)
      y = y+1

  if x < len(xlst):
    res += xlst[x:]
  if y < len(ylst):
    res += ylst[y:]

  return res

n = 1000
if (len(sys.argv) >= 2):
  n = int(sys.argv[1])
input = genlist(n)
sorted = mergesort(input)
for s in sorted:
  print s
