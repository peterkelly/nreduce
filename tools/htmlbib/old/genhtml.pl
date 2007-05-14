#!/usr/bin/perl


use Text::BibTeX;
use XML::Parser;
use XML::DOM;
use XML::Writer;

sub gettitles
{
  my $bibfilename = $_[0];
  my $titles = {};

  $bibfile = new Text::BibTeX::File $bibfilename;

  while ($entry = new Text::BibTeX::Entry $bibfile) {
    next unless $entry->parse_ok;
    my $title = $entry->get("title");
    $title =~ s/\{//g;
    $title =~ s/\}//g;
    $titles->{$entry->key} = $title;
  }

  return $titles;
}

sub gentoc
{
  my $writer = $_[0];
  my $parent = $_[1];
  my $depth = $_[2];
  my $titles = $_[3];
  my $papersdir = $_[4];
  my $pnum = $_[5];
  my $num = 1;
  my @children = $parent->getChildNodes;
  my $haveol = 0;

  foreach $child (@children) {
    if (($child->getNodeType == ELEMENT_NODE) &&
        ($child->getNodeName eq "section")) {
      my $headname = "h".$depth;
      if (!$haveol) {
        $writer->startTag("ol");
        $haveol = 1;
      }
      $writer->startTag("li");
      $writer->startTag("a", "href" => "#".$pnum.$num);
      $writer->characters($child->getAttribute("title"));
      $writer->endTag("a");
      gentoc($writer,$child,$depth+1,$titles,$papersdir,$pnum.$num.".");
      $writer->endTag("li");
      $num++;
    }
  }

  if ($haveol) {
      $writer->endTag("ol");
  }
}

sub gensections
{
  my $writer = $_[0];
  my $parent = $_[1];
  my $depth = $_[2];
  my $titles = $_[3];
  my $papersdir = $_[4];
  my $pnum = $_[5];
  my $num = 1;
  my @children = $parent->getChildNodes;

  $writer->startTag("p");
  foreach $child (@children) {
    if (($child->getNodeType == ELEMENT_NODE) &&
           ($child->getNodeName eq "item")) {
      my $key = $child->getAttribute("key");
      my $filename = "";
      if (-f "$papersdir/$key.pdf") {
        $filename = "$papersdir/$key.pdf";
      }
      elsif (-f "$papersdir/$key.ps.gz") {
        $filename = "$papersdir/$key.ps.gz";
      }
      if ($filename) {
        $writer->startTag("a", "href" => $filename);
        $writer->characters($titles->{$key});
        $writer->endTag("a");
        $writer->characters(sprintf(" [%s]",$key));
        $writer->startTag("br");
        $writer->endTag("br");
      }
      else {
        $writer->characters(sprintf("%s [%s]",$titles->{$key},$key));
        $writer->startTag("br");
        $writer->endTag("br");
      }
    }
  }
  $writer->endTag("p");

  foreach $child (@children) {
    if (($child->getNodeType == ELEMENT_NODE) &&
        ($child->getNodeName eq "section")) {
      my $headname = "h".$depth;
      $writer->startTag("a", "name" => $pnum.$num);
      $writer->endTag("a");
      $writer->startTag($headname);
      $writer->characters(sprintf("%s%d %s",$pnum,$num,$child->getAttribute("title")));
      $writer->endTag($headname);
      gensections($writer,$child,$depth+1,$titles,$papersdir,$pnum.$num.".");
      $num++;
    }
  }
}

sub genpage
{
  my $xmlfile = $_[0];
  my $bibfile = $_[1];
  my $papersdir = $_[2];
  my $titles = $_[3];

  my $parser = new XML::DOM::Parser;
  my $doc = $parser->parsefile($xmlfile);

  my $output = new IO::File(">literature.xml");

  print $output "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"\n";
  print $output "   \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n";

  my $writer = new XML::Writer(OUTPUT => $output);



  $writer->startTag("html","xmlns" => "http://www.w3.org/1999/xhtml");

  $writer->startTag("head");
  $writer->startTag("title");
  $writer->characters("Literature");
  $writer->endTag("title");
  $writer->endTag("head");

  $writer->startTag("body");

  $writer->startTag("h1", "style" => "text-align: center");
  $writer->characters("Literature");
  $writer->endTag("h1");

  my $literature = $doc->getDocumentElement;
  gentoc($writer,$literature,1,$titles,$papersdir,"");
  gensections($writer,$literature,1,$titles,$papersdir,"");


  $writer->endTag("body");

  $writer->endTag("html");
  $writer->end();
  $output->close();

  $doc->dispose;
}


my $allpapers = "/mnt/d/project/allpapers.lyx";
my $bibfilename = "/mnt/d/project/grid.bib";
my $papersdir = "/mnt/d/project/papers";
my $titles = gettitles($bibfilename);

genpage("allpapers.xml","grid.bib","papers",$titles);

