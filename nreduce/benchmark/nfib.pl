#!/usr/bin/perl

$max = $ARGV[0];

if ($max eq "") {
  print STDERR "no max value supplied\n";
  exit 1;
}

sub nfib
  {
    my $n = $_[0];
    if ($n <= 1) {
      return 1;
    }
    else {
      return nfib($n-2)+nfib($n-1);
    }
  }

for ($i = 0; $i <= $max; $i++) {
  printf("nfib(%d) = %d\n",$i,nfib($i));
}
