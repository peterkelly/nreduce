<?

function genlist($max)
{
  $res = array();
  for ($i = 0; $i < $max; $i++)
    $res[$i] = strrev(7*$i);
  return $res;
}

function mergesort($m)
{
  if (count($m) <= 1)
    return $m;

  $middle = count($m)/2;
  $left = array_slice($m,0,$middle);
  $right = array_slice($m,$middle,count($m));

  $left = mergesort($left);
  $right = mergesort($right);

  return merge($left,$right);
}


function merge($xlst,$ylst)
{
  $x = 0;
  $y = 0;
  $res = array();
  $rpos = 0;
  while (($x < count($xlst)) && ($y < count($ylst))) {
    $xval = $xlst[$x];
    $yval = $ylst[$y];
    if (strcmp($xval,$yval) < 0) {
      array_push($res,$xval);
      $x++;
    }
    else {
      array_push($res,$yval);
      $y++;
    }
  }
  while ($x < count($xlst)) {
    array_push($res,$xlst[$x]);
    $x++;
  }
  while ($y < count($ylst)) {
    array_push($res,$ylst[$y]);
    $y++;
  }

  return $res;
}

if ($argc >= 2)
  $n = $argv[1];
else
  $n = 1000;

$strings = genlist($n);
$sorted = mergesort($strings);
for ($i = 0; $i < count($sorted); $i++)
  print("$sorted[$i]\n");

?>
