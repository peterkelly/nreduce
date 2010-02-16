function node(left,right)
{
  this.left = left;
  this.right = right;
}

function bintree(depth)
{
  if (depth == 0)
    return null;
  else
    return new node(bintree(depth-1),bintree(depth-1));
}

function countnodes(tree)
{
  if (tree)
    return 1 + countnodes(tree.left) + countnodes(tree.right);
  else
    return 0;
}

var n = 16;
if (arguments.length > 0)
  n = arguments[0];

var tree = bintree(n);
var count = countnodes(tree);
print(count);
