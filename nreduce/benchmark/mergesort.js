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

function mergesort(m)
{
  if (m.length <= 1)
    return m;

  var middle = m.length/2;
  var left = m.slice(0,middle);
  var right = m.slice(middle,m.length);

  left = mergesort(left);
  right = mergesort(right);

  return merge(left,right);
}

function merge(xlst,ylst)
{
  var x = 0;
  var y = 0;
  var res = new Array();
  var rpos = 0;
  while ((x < xlst.length) && (y < ylst.length)) {
    var xval = xlst[x];
    var yval = ylst[y];
    if (xval < yval) {
      res.push(xval);
      x++;
    }
    else {
      res.push(yval);
      y++;
    }
  }
  if (x < xlst.length)
    res = res.concat(xlst.slice(x,xlst.length));
  if (y < ylst.length)
    res = res.concat(ylst.slice(y,ylst.length));
  return res;
}

var n = 1000;
if (arguments.length > 0)
  n = arguments[0];
var strings = genlist(n);
var sorted = mergesort(strings);
for (var i = 0; i < sorted.length; i++)
  print(sorted[i]);
