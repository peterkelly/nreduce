#!/usr/bin/python

import sys

def genlist(max):
  res = []
  for i in range(0,max):
    rev = str(7*i)[::-1]
    res.append(rev)
  return res

def mergesort(arr,aux,lo,hi):
  if lo >= hi:
    return

  mid = lo+(hi+1-lo)/2
  mergesort(arr,aux,lo,mid-1)
  mergesort(arr,aux,mid,hi)

  xlen = mid-lo
  ylen = hi+1-mid
  pos = 0
  x = lo
  y = mid
  while (x <= mid-1) and (y <= hi):
    xval = arr[x]
    yval = arr[y]
    if xval < yval:
      aux[pos] = xval
      pos = pos+1
      x = x+1
    else:
      aux[pos] = yval
      pos = pos+1
      y = y+1

  if x <= mid-1:
    aux[pos:pos+mid-x] = arr[x:mid]
    pos += mid-x

  if y <= hi:
    aux[pos:pos+hi+1-y] = arr[y:hi+1]
    pos += hi+1-y

  arr[lo:hi+1] = aux[0:hi+1-lo]

n = 1000
if (len(sys.argv) >= 2):
  n = int(sys.argv[1])
strings = genlist(n)
temp = [0]*n
mergesort(strings,temp,0,len(strings)-1)
for s in strings:
  print s
