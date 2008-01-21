#!/usr/bin/python

import sys

def bintree(depth,max):
  if depth == max:
    return None
  else:
    return (bintree(depth+1,max),bintree(depth+1,max))

def countnodes(tree):
  if tree is not None:
    return 1 + countnodes(tree[0]) + countnodes(tree[1])
  else:
    return 0

n = 16
if (len(sys.argv) >= 2):
  n = int(sys.argv[1])

tree = bintree(0,n)
count = countnodes(tree)
print count
