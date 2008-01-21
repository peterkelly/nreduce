#!/usr/bin/python

import sys

def createMatrix(height,width,val):
  matrix = [None]*height
  for y in range(0,height):
    matrix[y] = [0]*width
  for y in range(0,height):
    for x in range(0,width):
      matrix[y][x] = val%23
      val += 1
  return matrix

def printMatrix(matrix):
  height = len(matrix)
  width = len(matrix[0])

  for y in range(0,height):
    line = ""
    for x in range(0,height):
      line += "%5d "%(matrix[y][x])
    print line

def multiply(a,b):
  arows = len(a)
  acols = len(a[0])
  bcols = len(b[0])

  result = [None]*arows
  for y in range(0,arows):
    result[y] = [0]*bcols

  for y in range(0,arows):
    for x in range(0,bcols):
      for i in range(0,acols):
        result[y][x] += a[y][i] * b[i][x]

  return result

def matsum(matrix):
  height = len(matrix)
  width = len(matrix[0])
  total = 0
  for y in range(0,height):
    for x in range(0,width):
      total += matrix[y][x]
  return total

size = 10;
if (len(sys.argv) > 1):
  size = int(sys.argv[1])
doprint = (len(sys.argv) != 2)

a = createMatrix(size,size,1)
b = createMatrix(size,size,2)
if (doprint):
  printMatrix(a);
  print("--")
  printMatrix(b);
  print("--")

c = multiply(a,b);
if (doprint):
  printMatrix(c);
  print("");

print("sum = "+str(matsum(c)));
