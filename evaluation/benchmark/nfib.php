<?
function nfib($n)
{
  if ($n <= 1)
    return 1;
  else
    return nfib($n-2)+nfib($n-1);
}

if ($argc >= 2)
  $n = $argv[1];
else
  $n = 24;

for ($i = 0; $i <= $n; $i++)
  print("nfib($i) = ".nfib($i)."\n");
?>
