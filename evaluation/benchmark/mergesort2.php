<?

function genlist($max)
{
  $res = array();
  for ($i = 0; $i < $max; $i++)
    $res[$i] = strrev(7*$i);
  return $res;
}


function mergesort(&$arr,&$aux,$lo,$hi)
{
  if ($lo >= $hi)
    return;

  $mid = floor($lo+($hi+1-$lo)/2);
  mergesort($arr,$aux,$lo,$mid-1);
  mergesort($arr,$aux,$mid,$hi);

  $xlen = $mid-$lo;
  $ylen = $hi+1-$mid;
  $pos = 0;
  $x = $lo;
  $y = $mid;
  while (($x <= $mid-1) && ($y <= $hi)) {
    $xval = $arr[$x];
    $yval = $arr[$y];
    if (strcmp($xval,$yval) < 0) {
      $aux[$pos++] = $xval;
      $x++;
    }
    else {
      $aux[$pos++] = $yval;
      $y++;
    }
  }
  if ($x <= $mid-1) {
    for ($i = 0; $i < $mid-$x; $i++)
      $aux[$pos+$i] = $arr[$x+$i];
    $pos += $mid-$x;
  }
  if ($y <= $hi) {
    for ($i = 0; $i < $hi+1-$y; $i++)
      $aux[$pos+$i] = $arr[$y+$i];
    $pos += $hi+1-$y;
  }
  for ($i = 0; $i < $hi+1-$lo; $i++)
    $arr[$lo+$i] = $aux[$i];
}

if ($argc >= 2)
  $n = $argv[1];
else
  $n = 1000;

$strings = genlist($n);
$temp = array();
mergesort($strings,$temp,0,count($strings)-1);
for ($i = 0; $i < count($strings); $i++)
  print("$strings[$i]\n");

?>
