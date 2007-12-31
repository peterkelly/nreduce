#include <stdio.h>
#include <stdlib.h>

typedef struct bintree {
  struct bintree *left;
  struct bintree *right;
} bintree;

int countnodes(bintree *tree)
{
  if (tree)
    return 1 + countnodes(tree->left) + countnodes(tree->right);
  else
    return 0;
}

bintree *mktree(int depth, int max)
{
  if (depth == max) {
    return NULL;
  }
  else {
    bintree *tree = (bintree*)malloc(sizeof(bintree));
    tree->left = mktree(depth+1,max);
    tree->right = mktree(depth+1,max);
    return tree;
  }
}

void freetree(bintree *tree)
{
  if (tree) {
    freetree(tree->left);
    freetree(tree->right);
    free(tree);
  }
}

int main(int argc, char **argv)
{
  bintree *tree;
  int n = 16;
  if (2 <= argc)
    n = atoi(argv[1]);

  tree = mktree(0,n);
  printf("%d\n",countnodes(tree));
  freetree(tree);
  return 0;
}
