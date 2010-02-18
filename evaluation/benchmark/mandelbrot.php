<?

function mandel($Cr,$Ci)
{
  $Zr = 0.0;
  $Zi = 0.0;
  for ($res = 0; $res < 4096; $res++) {
    $newZr = (($Zr * $Zr) - ($Zi * $Zi)) + $Cr;
    $newZi = (2.0 * ($Zr * $Zi)) + $Ci;
    $mag = sqrt(($newZr * $newZr) + ($newZi * $newZi));
    if ($mag > 2.0)
      return $res;
    $Zr = $newZr;
    $Zi = $newZi;
  }
  return $res;
}

function printcell($num)
{
  if ($num > 40)
    return "  ";
  else if ($num > 30)
    return "''";
  else if ($num > 20)
    return "--";
  else if ($num > 10)
    return "**";
  else
    return "##";
}

function mloop($minx,$maxx,$miny,$maxy,$incr)
{
  for ($y = $miny; $y < $maxy; $y += $incr) {
    $line = "";
    for ($x = $minx; $x < $maxx; $x += $incr)
      $line .= printcell(mandel($x,$y));
    print("$line\n");
  }
}

if ($argc >= 2)
  $n = $argv[1];
else
  $n = 32;

$incr = 2.0/$n;
mloop(-1.5,0.5,-1.0,1.0,$incr);


?>
