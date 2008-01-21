#!/usr/bin/perl -w

sub createMatrix
{
  my ($height,$width,$val) = @_;
  my $matrix = [];
#  for (my $y = 0; $y < $height; $y++) {
#    $matrix->[$y] = [];
#  }

  for (my $y = 0; $y < $height; $y++) {
    for (my $x = 0; $x < $width; $x++) {
      $matrix->[$y]->[$x] = $val%23;
      $val++;
    }
  }
  return $matrix;
}

sub printMatrix
{
  my ($matrix) = @_;
  my $height = $#{$matrix}+1;
  my $width = $#{$matrix->[0]}+1;
  for (my $y = 0; $y < $height; $y++) {
    for (my $x = 0; $x < $width; $x++) {
      printf("%5d ",$matrix->[$y]->[$x]);
    }
    print "\n";
  }
}

sub multiply
{
  my ($a,$b) = @_;
  my $arows = $#{$a}+1;
  my $acols = $#{$a->[0]}+1;
  my $bcols = $#{$b->[0]}+1;

  my $result = [];

  for (my $y = 0; $y < $arows; $y++) {
    for (my $x = 0; $x < $bcols; $x++) {
      $result->[$y]->[$x] = 0;
      for (my $i = 0; $i < $acols; $i++) {
        $result->[$y]->[$x] += $a->[$y]->[$i] * $b->[$i]->[$x];
      }
    }
  }

  return $result;
}

sub matsum
{
  my ($matrix) = @_;
  my $total = 0;
  my $height = $#{$matrix}+1;
  my $width = $#{$matrix->[0]}+1;
  for (my $y = 0; $y < $height; $y++) {
    for (my $x = 0; $x < $width; $x++) {
      $total += $matrix->[$y]->[$x];
    }
  }
  return $total;
}


my $size = ($#ARGV < 0) ? 10 : $ARGV[0];
my $doprint = ($#ARGV != 0);

my $a = createMatrix($size,$size,1);
my $b = createMatrix($size,$size,2);
if ($doprint) {
  printMatrix($a);
  print "--\n";
  printMatrix($b);
  print "--\n";
 }

my $c = multiply($a,$b);
if ($doprint) {
  printMatrix($c);
  print "\n";
 }
print "sum = ".matsum($c)."\n";
