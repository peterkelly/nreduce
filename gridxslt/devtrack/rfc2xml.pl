#!/usr/bin/perl

use XML::Writer;


sub print_clause
{
  my $text = $clause;
  if ($text =~ /^\s*$/s) {
    return;
  }
  $text =~ s/^\s*//s;
  $text =~ s/\s*$//s;
  $writer->startTag("clause","name" => $id."-".(++$idcounts{$id}), "text" => substr($text,0,80));
  $writer->endTag("clause");
}


$filename = $ARGV[0];
$specname = $ARGV[1];
$specurl = $ARGV[2];
$spectitle = $ARGV[3];
if ($filename eq "" || $specname eq "" || $specurl eq "" || $spectitle eq "") {
  print "Usage: rfc2xml <filename> <specname> <spectitle> <specurl>\n";
  exit 1;
}

$writer = XML::Writer->new();

$writer->xmlDecl();
$writer->startTag("spec","xmlns" => "http://gridxslt.sourceforge.net/devtrack",
                         "name" => $specname,
                         "url" => $specurl,
                         "title" => $spectitle);
$insection = 0;

$id = "";
$clauseno = 0;
$clause = "";

open(INPUT,$filename) || die "Can't open $filename";
while ($line = <INPUT>) {
  $line =~ s/\r?\n$//;
  $line =~ s/\x0c//;
  if ($line =~ /\[Page \d+\]\s*$/) {
  }
  elsif ($line =~ /^RFC \d+.*\d+\s*/) {
  }
  elsif ($line =~ /^(\d(\d|\.)*)\s+(.*)\s*$/) {
    my $number = $1;
    my $title = $3;
    my $name = $title;
    $name =~ s/\s+/-/g;
    $name =~ tr/A-Z/a-z/;
    $name =~ s/[^A-Za-z0-9-]//;

    $id = $name;
    $clauseno = 0;
    $clause = "";

    $id =~ s/^(.*?)-(.*?)-(.*?)-.*$/$1-$2-$3/;

    if ($insection) {
      $writer->endTag("section");
    }
    $writer->startTag("section","number" => $number, "name" => $name, "title" => $title);
    $insection = 1;
  }
  elsif ($line =~ /^\s*$/) {
    if ($insection) {
      print_clause();
      $clause = "";
    }
  }
  else {
    $clause .= $line;
  }
}

if ($insection) {
  $writer->endTag("section");
}

close (INPUT);

$writer->endTag("spec");
$writer->end();
