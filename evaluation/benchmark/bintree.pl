#!/usr/bin/perl -w

sub bintree
{
  my $depth = $_[0];

  if ($depth == 0) {
    return undef;
  }
  else {
    return [ bintree($depth-1), bintree($depth-1) ];
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
my $tree = bintree($n);
my $count = countnodes($tree);
print "$count\n";
