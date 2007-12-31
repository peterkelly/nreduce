#!/usr/bin/python

# The Computer Language Benchmarks Game
# http://shootout.alioth.debian.org/
# Written by Dima Dorfman, 2004

# modified by Heinrich Acker
# modified by Dani Nanz 2007-10-03

import sys
import math
from itertools import count, islice, izip


def nsieve(m, c=0):
  prime = [True] * (m + 1)
  iu = math.sqrt(m)
  count = 0
  for i in xrange(2,m):
    if prime[i]:
      for k in xrange(i+i,m,i):
          prime[k] = False
      count = count+1
  print 'Primes up to %d: %d' % (m, count)

nval = 10000
if (len(sys.argv) >= 2):
  nval = int(sys.argv[1])

nsieve(nval)
