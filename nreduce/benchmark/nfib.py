#!/usr/bin/python

import sys

def nfib(n):
    if n <= 1:
        return 1
    return nfib(n-2)+nfib(n-1)

n = 24
if (len(sys.argv) >= 2):
  n = int(sys.argv[1])

for i in range(int(n)+1):
    print "nfib(%d) = %d" % (i,nfib(i))
