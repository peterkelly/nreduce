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
  my @m = @_;

  if (scalar(@m) <= 1) {
    return @m;
  }

  my $middle = scalar(@m)/2;
  my @left = @m[0..$middle-1];
  my @right = @m[$middle..$#m];

  @left = mergesort(@left);
  @right = mergesort(@right);

  return merge(\@left,\@right);
}

sub merge {
  my ($xref,$yref) = @_;
  my @xlst = @{$xref};
  my @ylst = @{$yref};
  my $x = 0;
  my $y = 0;
  my @res = ();

  while (($x < scalar(@xlst)) && ($y < scalar(@ylst))) {
    my $xval = $xlst[$x];
    my $yval = $ylst[$y];
    if ($xval lt $yval) {
      push(@res,$xval);
      $x++;
    }
    else {
      push(@res,$yval);
      $y++;
    }
  }
  if ($x < scalar(@xlst)) {
    push(@res,@xlst[$x..$#xlst]);
  }
  if ($y < scalar(@ylst)) {
    push(@res,@ylst[$y..$#ylst]);
  }
  return @res;
}

my $n = ($#ARGV < 0) ? 1000 : $ARGV[0];
my @input = genlist($n);
my @sorted = mergesort(@input);
foreach $str (@sorted) {
  print "$str\n";
}
