#!/usr/bin/perl -w

sub genlist
{
  my ($max) = @_;
  my @res = ();

  for (my $i = 0; $i < $max; $i++) {
    my $str = 7*$i;
    my $rev = reverse($str);
    push(@res,$rev);
  }

  return @res;
}

sub quicksort
{
  my @list = @_;

  my $pivot = $list[0];

  my @before = ();
  my @after = ();

  for (my $pos = 1; $pos <= $#list; $pos++) {
    my $cur = $list[$pos];
    if ($cur lt $pivot) {
      push(@before,$cur);
    }
    else {
      push(@after,$cur);
    }
  }

  my @res = ();
  if ($#before >= 0) {
    push(@res,quicksort(@before));
  }
  push(@res,$pivot);
  if ($#after >= 0) {
    push(@res,quicksort(@after));
  }

  return @res;
}

my $n = ($#ARGV < 0) ? 1000 : $ARGV[0];
my @input = genlist($n);
my @sorted = quicksort(@input);
foreach $str (@sorted) {
  print "$str\n";
}
