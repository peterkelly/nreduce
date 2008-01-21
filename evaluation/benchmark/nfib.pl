#!/usr/bin/perl

sub nfib
{
  my ($n) = @_;
  if ($n <= 1) {
    return 1;
  }
  else {
    return nfib($n-2)+nfib($n-1);
  }
}

my $n = ($#ARGV < 0) ? 24 : $ARGV[0];
for ($i = 0; $i <= $n; $i++) {
  printf("nfib(%d) = %d\n",$i,nfib($i));
}
