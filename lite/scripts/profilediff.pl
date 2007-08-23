#!/usr/bin/perl

$old_filename = $ARGV[0];
$new_filename = $ARGV[1];

if (($old_filename eq "") || ($new_filename eq "")) {
  print STDERR "Usage: profilediff.pl <old filename> <new filename>\n";
  exit 1
}

print "Comparing $old_filename and $new_filename\n\n";

open(OLD,$old_filename) || die "Can't open $old_filename";
open(NEW,$new_filename) || die "Can't open $new_filename";

printf("%-30s %-10s %-10s %-10s %7s\n","Statistic","Old","New","Difference","Pct");
printf("%-30s %-10s %-10s %-10s %7s\n","---------","---","---","----------","---");

$done = 0;
while (!$done) {
  $oldline = <OLD>;
  $newline = <NEW>;

  if ($oldline =~ /^([A-Za-z ]+)\s+(\d+)$/) {
    $stat1 = $1;
    $val1 = $2;
    die unless $newline =~ /^([A-Za-z ]+)\s+(\d+)$/;
    $stat2 = $1;
    $val2 = $2;
    $stat1 eq $stat2 || die "Statistic names not equal: $stat1/$stat2";
    $stat1 =~ s/\s+$//;
    $diff = $val2 - $val1;
    if ($val1 > 0) {
      $pct = 100.0*$val2/$val1;
    } else {
      $pct = 0;
    }
    printf("%-30s %-10d %-10d %-10d %6.2f%\n",$stat1,$val1,$val2,$diff,$pct);
  }

  if ($oldline =~ /Function usage/) {
    last;
  }
}

close(OLD);
close(NEW);
