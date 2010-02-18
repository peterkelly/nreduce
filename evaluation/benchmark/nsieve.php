<?

// Based on the nsieve program from http://shootout.alioth.debian.org/ by Alexei Svitkine

function nsieve($n)
{
  $prime = array();
  $count = 0;

  for ($i = 2; $i <= $n; $i++)
    $prime[$i] = true;

  for ($i = 2; $i <= $n; $i++) {
    if ($prime[$i]) {
      for ($k = $i+i; $k <= $n; $k += $i)
        $prime[$k] = false;
      $count++;
    }
  }
  return $count;
}

if ($argc >= 2)
  $n = $argv[1];
else
  $n = 10000;

print("Primes up to $n: ".nsieve($n)."\n");

?>
