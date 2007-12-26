#!/usr/bin/perl -w

sub mandel
{
  my ($Cr,$Ci,$iterations) = @_;
  my $Zr = 0.0;
  my $Zi = 0.0;
  for (my $r = 0; $r < $iterations; $r++) {
    $newZr = ($Zr*$Zr) - ($Zi*$Zi) + $Cr;
    $newZi = (2.0*($Zr*$Zi)) + $Ci;
    $mag = sqrt(($newZr*$newZr)+($newZi*$newZi));
    if ($mag > 2.0) {
      return $r;
    }
    $Zr = $newZr;
    $Zi = $newZi;
  }
  return $iterations;
}


sub printcell
{
  my $num = $_[0];
  if ($num > 40) {
    print "  ";
  }
  elsif ($num > 30) {
    print "''";
  }
  elsif ($num > 20) {
    print "--";
  }
  elsif ($num > 10) {
    print "**";
  }
  else {
    print "##";
  }
}

sub mloop
{
  my ($minx,$maxx,$xincr,$miny,$maxy,$yincr) = @_;
  $maxx += $xincr;
  for ($y = $miny; $y <= $maxy; $y += $yincr) {
    for ($x = $minx; $x <= $maxx; $x += $xincr) {
      printcell(mandel($x,$y,4096));
    }
    print "\n";
  }
}

$incr = 0.01;
if (0 <= $#ARGV) {
  $incr = $ARGV[0];
}
print "incr = $incr\n";
mloop(-1.5,0.5,$incr,-1.0,1.0,$incr);
