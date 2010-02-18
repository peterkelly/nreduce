<?

function createMatrix($height,$width,$val)
{
  $matrix = array();
  for ($y = 0; $y < $height; $y++)
    $matrix[$y] = array();
  for ($y = 0; $y < $height; $y++) {
    for ($x = 0; $x < $width; $x++) {
      $matrix[$y][$x] = $val%23;
      $val++;
    }
  }
  return $matrix;
}

function printMatrix($matrix)
{
  $height = count($matrix);
  $width = count($matrix[0]);
  for ($y = 0; $y < $height; $y++) {
    $line = "";
    for ($x = 0; $x < $width; $x++) {
      $str = "".$matrix[$y][$x];
      for ($i = 0; $i < 5-strlen($str); $i++)
        $line .= " ";
      $line .= $str;
      $line .= " ";
    }
    print("$line\n");
  }
}

function multiply($a,$b)
{
  $arows = count($a);
  $acols = count($a[0]);
  $bcols = count($b[0]);

  $result = array();
  for ($y = 0; $y < $arows; $y++)
    $result[$y] = array();

  for ($y = 0; $y < $arows; $y++) {
    for ($x = 0; $x < $bcols; $x++) {
      $result[$y][$x] = 0;
      for ($i = 0; $i < $acols; $i++)
        $result[$y][$x] += $a[$y][$i] * $b[$i][$x];
    }
  }

  return $result;
}

function matsum($matrix)
{
  $total = 0;
  for ($y = 0; $y < count($matrix); $y++)
    for ($x = 0; $x < count($matrix[0]); $x++)
      $total += $matrix[$y][$x];
  return $total;
}

if ($argc >= 2)
  $size = $argv[1];
else
  $size = 10;

$doprint = ($argc != 2);

$a = createMatrix($size,$size,1);
$b = createMatrix($size,$size,2);
if ($doprint) {
  printMatrix($a);
  print("--\n");
  printMatrix($b);
  print("--\n");
}

$c = multiply($a,$b);
if ($doprint) {
  printMatrix($c);
  print("\n");
}
print("sum = ".matsum($c)."\n");

?>
