#!/usr/bin/perl -w

# The Computer Language Benchmarks Game
# http://shootout.alioth.debian.org/
#
# contributed by David Pyke, March 2005
# optimized by Steffen Mueller, Sept 2007

use integer;
use strict;

sub nsieve {
   my ($m) = @_;
   my @a = (1) x $m;

   my $count = 0;
   foreach my $i (2..$m-1) {
      if ($a[$i]) {
         for (my $j = $i + $i; $j < $m; $j += $i){
            $a[$j] = 0;
         }
         ++$count;
      }
   }
   return $count;
}


sub nsieve_test {
   my($n) = @_;

   my $m = $n;
   my $ncount= nsieve($m);
   printf "Primes up to %u: %u\n", $m, $ncount;
}

my $N;
if ($#ARGV < 0) {
  $N = 10000;
}
else {
  $N = $ARGV[0];
}
nsieve_test($N);
