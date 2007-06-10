function mandel(Zr,Zi,Cr,Ci,iterations,res)
{
  if (res == iterations)
    res;
  else {
    let newZr = Zr*Zr - Zi*Zi + Cr;
    let newZi = 2.0*Zr*Zi + Ci;
    let mag = sqrt(newZr*newZr + newZi*newZi);
    if (mag > 2.0)
      res;
    else
      mandel(newZr,newZi,Cr,Ci,iterations,res+1);
  }
}

function mandel0(Cr,Ci,iterations)
{
  mandel(0.0,0.0,Cr,Ci,iterations,0);
}

function printcell(num)
{
  if (num > 40)
    "  ";
  else if (num > 30)
    "''";
  else if (num > 20)
    "--";
  else if (num > 10)
    "**";
  else
    "##";
}

function mloop(minx,maxx,xincr,miny,maxy,yincr,x,y)
{
  if (x > maxx) {
    if (y > maxy)
      "\n";
    else
      "\n".mloop(minx,maxx,xincr,miny,maxy,yincr,minx,y+yincr);
  }
  else {
    printcell(mandel0(x,y,50)).mloop(minx,maxx,xincr,miny,maxy,yincr,x+xincr,y);
  }
}

function mandelrange(minx,maxx,xincr,miny,maxy,yincr)
{
  mloop(minx,maxx+xincr,xincr,miny,maxy,yincr,minx,miny);
}

function main()
{
  let minx = -1.5;
  let maxx = 0.5;
  let xincr = 0.046;
  let miny = -1.0;
  let maxy = 1.0;
  let yincr = 0.046;
  mandelrange(minx,maxx,xincr,miny,maxy,yincr);
}
