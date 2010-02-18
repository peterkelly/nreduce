<?

function genlist($max)
{
  $res = array();
  for ($i = 0; $i < $max; $i++)
    $res[$i] = strrev(7*$i);
  return $res;
}

function quicksort(&$arr,$left,$right)
{
  $store = $left;
  $pivot = $arr[$right];
  for ($i = $left; $i < $right; $i++) {
    if (strcmp($arr[$i],$pivot) < 0) {
      $temp = $arr[$store];
      $arr[$store] = $arr[$i];
      $arr[$i] = $temp;
      $store++;
    }
  }
  $arr[$right] = $arr[$store];
  $arr[$store] = $pivot;

  if ($left < $store-1)
    quicksort($arr,$left,$store-1);
  if ($right > $store+1)
    quicksort($arr,$store+1,$right);
}

if ($argc >= 2)
  $n = $argv[1];
else
  $n = 1000;

$strings = genlist($n);
quicksort($strings,0,count($strings)-1);
for ($i = 0; $i < count($strings); $i++)
  print("$strings[$i]\n");

?>
