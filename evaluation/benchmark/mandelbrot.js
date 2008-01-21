function mandel(Cr,Ci)
{
  var res;
  var Zr = 0.0;
  var Zi = 0.0;
  for (res = 0; res < 4096; res++) {
    var newZr = ((Zr * Zr) - (Zi * Zi)) + Cr;
    var newZi = (2.0 * (Zr * Zi)) + Ci;
    var mag = Math.sqrt((newZr * newZr) + (newZi * newZi));
    if (mag > 2.0)
      return res;
    Zr = newZr;
    Zi = newZi;
  }
  return res;
}

function printcell(num)
{
  if (num > 40)
    return "  ";
  else if (num > 30)
    return "''";
  else if (num > 20)
    return "--";
  else if (num > 10)
    return "**";
  else
    return "##";
}

function mloop(minx,maxx,miny,maxy,incr)
{
  for (var y = miny; y < maxy; y += incr) {
    var line = "";
    for (var x = minx; x < maxx; x += incr)
      line += printcell(mandel(x,y));
    print(line);
  }
}

var n = 32;
if (arguments.length > 0)
  n = arguments[0];

var incr = 2.0/n;
mloop(-1.5,0.5,-1.0,1.0,incr);
