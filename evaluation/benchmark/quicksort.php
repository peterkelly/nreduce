<?

function genlist($max)
{
  $res = array();
  for ($i = 0; $i < $max; $i++)
    $res[$i] = strrev(7*$i);
  return $res;
}

function quicksort($list)
{
  $pivot = $list[0];
  $before = array();
  $after = array();
  for ($pos = 1; $pos < count($list); $pos++) {
    $cur = $list[$pos];
    if (strcmp($cur,$pivot) < 0)
      array_push($before,$cur);
    else
      array_push($after,$cur);
  }
  $res = array();
  if (count($before) > 0)
    $res = array_merge($res,quicksort($before));
  array_push($res,$pivot);
  if (count($after) > 0)
    $res = array_merge($res,quicksort($after));
  return $res;
}

if ($argc >= 2)
  $n = $argv[1];
else
  $n = 1000;

$strings = genlist($n);
$sorted = quicksort($strings);
for ($i = 0; $i < count($sorted); $i++)
  print("$sorted[$i]\n");

?>
