#!/usr/bin/perl -w

sub mandel
{
  my ($Cr,$Ci) = @_;
  my $Zr = 0.0;
  my $Zi = 0.0;
  my $res;
  for ($res = 0; $res < 4096; $res++) {
    $newZr = ($Zr*$Zr) - ($Zi*$Zi) + $Cr;
    $newZi = (2.0*($Zr*$Zi)) + $Ci;
    $mag = sqrt(($newZr*$newZr)+($newZi*$newZi));
    if ($mag > 2.0) {
      return $res;
    }
    $Zr = $newZr;
    $Zi = $newZi;
  }
  return $res;
}

sub printcell
{
  my ($num) = @_;
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
  my ($minx,$maxx,$miny,$maxy,$incr) = @_;
  for ($y = $miny; $y < $maxy; $y += $incr) {
    for ($x = $minx; $x < $maxx; $x += $incr) {
      printcell(mandel($x,$y));
    }
    print "\n";
  }
}

my $n = ($#ARGV < 0) ? 32 : $ARGV[0];
my $incr = 2.0/$n;

mloop(-1.5,0.5,-1.0,1.0,$incr);
