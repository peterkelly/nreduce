<?

class Node
{
  public $left;
  public $right;

  function __construct($left,$right)
  {
    $this->left = $left;
    $this->right = $right;
  }
}

function bintree($depth)
{
  if ($depth == 0)
    return null;
  else
    return new Node(bintree($depth-1),bintree($depth-1));
}

function countnodes($tree)
{
  if ($tree != null)
    return 1 + countnodes($tree->left) + countnodes($tree->right);
  else
    return 0;
}

if ($argc >= 2)
  $n = $argv[1];
else
  $n = 16;

$tree = bintree($n);
$count = countnodes($tree);
print("$count\n");

?>
