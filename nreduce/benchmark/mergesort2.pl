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

sub mergesort
{
  my ($arr,$aux,$lo,$hi) = @_;

  if ($lo >= $hi) {
    return;
  }

  my $mid = int($lo+($hi+1-$lo)/2);
  mergesort($arr,$aux,$lo,$mid-1);
  mergesort($arr,$aux,$mid,$hi);

  my $xlen = $mid-$lo;
  my $ylen = $hi+1-$mid;
  my $pos = 0;
  my $x = $lo;
  my $y = $mid;
  while (($x <= $mid-1) && ($y <= $hi)) {
    my $xval = $arr->[$x];
    my $yval = $arr->[$y];
    if ($xval lt $yval) {
      $aux->[$pos++] = $xval;
      $x++;
    }
    else {
      $aux->[$pos++] = $yval;
      $y++;
    }
  }

  my $i;

  if ($x <= $mid-1) {
    for ($i = 0; $i < $mid-$x; $i++) {
      $aux->[$pos+$i] = $arr->[$x+$i];
    }
    $pos += $mid-$x;
  }

  if ($y <= $hi) {
    for ($i = 0; $i < $hi+1-$y; $i++) {
      $aux->[$pos+$i] = $arr->[$y+$i];
    }
    $pos += $hi+1-$y;
  }
  for ($i = 0; $i < $hi+1-$lo; $i++) {
    $arr->[$lo+$i] = $aux->[$i];
  }
}


my $n = ($#ARGV < 0) ? 1000 : $ARGV[0];
my @strings = genlist($n);
my @temp = ();
mergesort(\@strings,\@temp,0,scalar(@strings)-1);
foreach $str (@strings) {
  print "$str\n";
}
