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
  my ($arr,$left,$right) = @_;

  my $store = $left;
  my $pivot = $arr->[$right];
  for (my $i = $left; $i < $right; $i++) {
    if ($arr->[$i] lt $pivot) {
      my $temp = $arr->[$store];
      $arr->[$store] = $arr->[$i];
      $arr->[$i] = $temp;
      $store++;
    }
  }
  $arr->[$right] = $arr->[$store];
  $arr->[$store] = $pivot;

  if ($left < $store-1) {
    quicksort($arr,$left,$store-1);
  }
  if ($right > $store+1) {
    quicksort($arr,$store+1,$right);
  }
}

my $n = ($#ARGV < 0) ? 1000 : $ARGV[0];
my @strings = genlist($n);
my @sorted = @{quicksort(\@strings,0,$#strings)};
foreach $str (@strings) {
  print "$str\n";
}
