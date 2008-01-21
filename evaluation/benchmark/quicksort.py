#!/usr/bin/python

import sys

def genlist(max):
  res = []
  for i in range(0,max):
    rev = str(7*i)[::-1]
    res.append(rev)
  return res

def quicksort(list):
  pivot = list[0]
  before = []
  after = []

  for pos in range(1,len(list)):
    cur = list[pos]
    if cur < pivot:
      before.append(cur)
    else:
      after.append(cur)

  res = []
  if len(before) > 0:
    res += quicksort(before)
  res.append(pivot)
  if len(after) > 0:
    res += quicksort(after)

  return res

n = 1000
if (len(sys.argv) >= 2):
  n = int(sys.argv[1])
input = genlist(n)
sorted = quicksort(input)
for s in sorted:
  print s
