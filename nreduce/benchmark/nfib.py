#!/usr/bin/python
import sys

def nfib(n):
    if n <= 1:
        return 1
    return nfib(n-2)+nfib(n-1)

if len(sys.argv) < 2:
    sys.stderr.write("no max value supplied\n")
    sys.exit(-1)

max = sys.argv[1]

for i in range(int(max)+1):
    print "nfib(%d) = %d" % (i,nfib(i))
