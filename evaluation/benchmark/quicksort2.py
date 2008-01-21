#!/usr/bin/python

import sys

def genlist(max):
  res = []
  for i in range(0,max):
    rev = str(7*i)[::-1]
    res.append(rev)
  return res

def quicksort(arr,left,right):
  store = left
  pivot = arr[right]

  for i in range(left,right):
    if arr[i] < pivot:
      temp = arr[store]
      arr[store] = arr[i]
      arr[i] = temp
      store = store + 1

  arr[right] = arr[store]
  arr[store] = pivot

  if (left < store-1):
    quicksort(arr,left,store-1)
  if (right > store+1):
    quicksort(arr,store+1,right)

n = 1000
if (len(sys.argv) >= 2):
  n = int(sys.argv[1])
strings = genlist(n)
quicksort(strings,0,len(strings)-1)
for s in strings:
  print s
