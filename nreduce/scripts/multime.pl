#!/usr/bin/perl

if (($#ARGV)+1 < 2) {
  print STDERR "Usge: multime <count> cmd...\n";
  exit(1);
}

$count = shift(@ARGV);
@cmd=@ARGV;

$total = 0;

for ($i = 0; $i < $count; $i++) {

  my $data = "";
  open(FILE,"bash -c 'time @cmd' 2>&1 >multime.out |") || die "Can't run command";
  while (<FILE>) {
    $data .= $_;
  }
  close(FILE);

  $data =~ /user\s+0m(.*?)s/ || die "Output does not contain user time measure";
  my $time = $1;
  printf("time: %.3f\n",$time);
  $total += $time;
}

$avg = $total/$count;

printf("Average: %.3f\n",$avg);
