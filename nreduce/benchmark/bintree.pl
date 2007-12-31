#!/usr/bin/perl -w

sub bintree
{
  my ($depth,$max) = @_;

  if ($depth == $max) {
    return undef;
  }
  else {
    return [ bintree($depth+1,$max), bintree($depth+1,$max) ];
  }
}

sub countnodes
{
  my ($tree) = @_;
  if ($tree) {
    return 1+countnodes($tree->[0])+countnodes($tree->[1]);
  }
  else {
    return 0;
  }
}

my $n = ($#ARGV < 0) ? 16 : $ARGV[0];
my $tree = bintree(0,$n);
my $count = countnodes($tree);
print "$count\n";
