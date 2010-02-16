public class bintree
{
  bintree left;
  bintree right;

  public bintree(bintree left, bintree right)
  {
    this.left = left;
    this.right = right;
  }

  static bintree mktree(int depth)
  {
    if (depth == 0)
      return null;
    else
      return new bintree(mktree(depth-1),mktree(depth-1));
  }

  static int countnodes(bintree tree)
  {
    if (tree != null)
      return 1 + countnodes(tree.left) + countnodes(tree.right);
    else
      return 0;
  }

  public static void main(String[] args)
  {
    int n = 16;
    if (args.length > 0)
      n = Integer.parseInt(args[0]);
    bintree tree = mktree(n);
    int count = countnodes(tree);
    System.out.println(count);
  }
}
