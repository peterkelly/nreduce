function createMatrix(height,width,val)
{
  var matrix = new Array(height);
  for (var y = 0; y < height; y++)
    matrix[y] = new Array(width);
  for (var y = 0; y < height; y++) {
    for (var x = 0; x < width; x++) {
      matrix[y][x] = val%23;
      val++;
    }
  }
  return matrix;
}

function printMatrix(matrix)
{
  var height = matrix.length;
  var width = matrix[0].length;
  for (var y = 0; y < height; y++) {
    var line = "";
    for (var x = 0; x < width; x++) {
      var str = ""+matrix[y][x];
      for (var i = 0; i < 5-str.length; i++)
        line += " ";
      line += str;
      line += " ";
    }
    print(line);
  }
}

function multiply(a,b)
{
  var arows = a.length;
  var acols = a[0].length;
  var bcols = b[0].length;

  var result = new Array(arows);
  for (var y = 0; y < arows; y++)
    result[y] = new Array(bcols);

  for (var y = 0; y < arows; y++) {
    for (var x = 0; x < bcols; x++) {
      result[y][x] = 0;
      for (var i = 0; i < acols; i++)
        result[y][x] += a[y][i] * b[i][x];
    }
  }

  return result;
}

function matsum(matrix)
{
  var total = 0;
  for (var y = 0; y < matrix.length; y++)
    for (var x = 0; x < matrix[0].length; x++)
      total += matrix[y][x];
  return total;
}

var size = 10;
if (arguments.length > 0)
  size = arguments[0];
var doprint = (arguments.length != 1);

var a = createMatrix(size,size,1);
var b = createMatrix(size,size,2);
if (doprint) {
  printMatrix(a);
  print("--");
  printMatrix(b);
  print("--");
}

var c = multiply(a,b);
if (doprint) {
  printMatrix(c);
  print("");
}
print("sum = "+matsum(c));
