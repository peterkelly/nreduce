function genlist(max)
{
  var res = new Array(max);
  for (var i = 0; i < max; i++) {
    var chars = (7*i)+"";
    var rev = "";
    for (var c = 0; c < chars.length; c++)
      rev += chars[chars.length-1-c];
    res[i] = rev;
  }
  return res;
}

function mergesort(arr,aux,lo,hi)
{
  if (lo >= hi)
    return;

  var mid = Math.floor(lo+(hi+1-lo)/2);
  mergesort(arr,aux,lo,mid-1);
  mergesort(arr,aux,mid,hi);

  var xlen = mid-lo;
  var ylen = hi+1-mid;
  var pos = 0;
  var x = lo;
  var y = mid;
  while ((x <= mid-1) && (y <= hi)) {
    var xval = arr[x];
    var yval = arr[y];
    if (xval < yval) {
      aux[pos++] = xval;
      x++;
    }
    else {
      aux[pos++] = yval;
      y++;
    }
  }
  if (x <= mid-1) {
    for (var i = 0; i < mid-x; i++)
      aux[pos+i] = arr[x+i];
    pos += mid-x;
  }
  if (y <= hi) {
    for (var i = 0; i < hi+1-y; i++)
      aux[pos+i] = arr[y+i];
    pos += hi+1-y;
  }
  for (var i = 0; i < hi+1-lo; i++)
    arr[lo+i] = aux[i];
}

var n = 1000;
if (arguments.length > 0)
  n = arguments[0];
var strings = genlist(n);
var temp = new Array(n);
mergesort(strings,temp,0,strings.length-1);
for (var i = 0; i < strings.length; i++)
  print(strings[i]);
