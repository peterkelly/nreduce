#!/usr/bin/perl -w

#
# This file is part of the GridXSLT project
# Copyright (C) 2005 Peter Kelly (pmk@cs.adelaide.edu.au)
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# $Id$
#

use POSIX;

main();

sub transform_point {
  my $point = $_[0];
  my $transform = $_[1];

  return { 'x' => $transform->{'xoff'} + $point->{'x'},
           'y' => $transform->{'height'} - ($transform->{'yoff'} + $point->{'y'}) };
}

sub convert_points {
  my $str = $_[0];
  my $transform = $_[1];

  $str =~ /^e\,(\d+\,\d+)\s+(((\d+\,\d+)\s*)+)$/ || die "Invalid line str: $str";

  my $first1 = $1;
  my @rest1 = split(/\s+/,$2);

  my $first = transform_point(parse_point($first1),$transform);
  my @rest = map { transform_point(parse_point($_),$transform); } @rest1;

  my $ret = "M ".point_str($first)." L ".join(" L ",
                                              (map { point_str($_); } @rest),
                                              (map { point_str($_); } reverse(@rest)));

  return $ret;
}

sub parse_color {
  my $str = $_[0];

  $str =~ /^#([a-fA-F0-9]{2})([a-fA-F0-9]{2})([a-fA-F0-9]{2})$/ || die "Invalid color: $str";
  return { 'red' => hex($1), 'green' => hex($2), 'blue' => hex($3) };
}

sub main {
  my @dots = parse_dot_dir("steps");

  annotate_dots(@dots);
#  dump_dots(@dots);
  my @nodenames = get_all_nodenames(@dots);
  my @edgenames = get_all_edgenames(@dots);

#  foreach my $dot (@dots) {
#    print "Step bounds: ".rect_str($dot->{'bounds'})."\n";
#  }

  my @stepbounds = map { $_->{'bounds'} } @dots;
  my $animbounds = bounding_rect(@stepbounds);
  my $animsize = rect_size($animbounds);
  print ".flash bbox=$animsize->{'width'}x".($animsize->{'height'}+50)."\n";
  print ".box background $animsize->{'width'} ".($animsize->{'height'}+50)." color=white fill=white\n";
  print ".put background\n";
  print ".font arial \"arial.ttf\"\n";

  print ".edittext title text=\"Title\" font=arial color=black size=12pt width=$animsize->{'width'} height=$animsize->{'height'}\n";
  print ".put title x=0 y=$animsize->{'height'}\n";

  # create node objects (one per node)
  foreach my $nodename (@nodenames) {
    print ".box $nodename width=100 height=100 color=#0000FF fill=#FFFFFF\n";
  }

  # create edge and text objects
  my $stepno = 0;
  foreach my $dot (@dots) {
    my $nodes = $dot->{'nodes'};
    my $edges = $dot->{'edges'};
    my $transform = $dot->{'transform'};

    foreach my $edgename (keys(%$edges)) {
      my $edge = $edges->{$edgename};
      print ".outline ${edgename}___outline$stepno:\n";
      print "  ".convert_points($edge->{'pos'},$transform)."\n";
      print ".end\n";
      print ".filled ${edgename}___filled$stepno outline=${edgename}___outline$stepno color=black fill=white\n";
    }

    foreach my $nodename (keys(%$nodes)) {
      my $node = $nodes->{$nodename} || die "no such node";
      if (!ignore_node($nodename)) {
        my $label = $node->{'label'} || die "no label for $nodename";
        $label =~ s/^\{(.*?)\|.*$/$1/;
#        print "node $nodename label $label\n";
        print ".text ${nodename}___$stepno font=arial text=\"$label\" color=black size=12pt\n";
      }
    }

    $stepno++;
  }


#  foreach my $edgename (@edgenames) {
#    print "$edgename\n";
#  }






  $stepno = 0;
  my $frameno = 0;
  my @prevnodes = ();
  my @prevedges = ();
  my $prevedges = undef;


  $frameno += 1;
  print ".frame $frameno\n";
  print ".action:\n";
  print "title.htmlText=\"step $stepno, frame $frameno\";\n";
  print ".end\n";


  foreach my $dot (@dots) {

    my @thisnodes = ();
    my $nodes = $dot->{'nodes'};
    my $edges = $dot->{'edges'};

    my $bottom = $dot->{'bounds'}->{'bottom'};
    my $transform = $dot->{'transform'};

    foreach my $nodename (keys(%$nodes)) {
      if (!ignore_node($nodename)) {
        push(@thisnodes,$nodename);
      }
    }

    foreach my $nodename (@prevnodes) {
      if (!grep(/^$nodename$/,@thisnodes)) {
        print ".del $nodename\n";
      }
      print ".del ${nodename}___".($stepno-1)."\n";
    }
    foreach my $edgename (keys(%$prevedges)) {
      print ".del ${edgename}___filled".($stepno-1)."\n";
    }

    foreach my $nodename (keys(%$nodes)) {
      if (!ignore_node($nodename)) {
        my $node = $nodes->{$nodename};
        my $bounds = $node->{'_bounds'} || die "$dot->{'filename'}: node $nodename has no bounds";
        my $size = rect_size($bounds);
        my $verb = "change";

        if (!grep(/^$nodename$/,@prevnodes)) {
          $verb = "put";
        }
        else {
          $verb = "change";
        }

        my $fillstr = "red=100% green=100% blue=100%";

        if ($node->{'fillcolor'}) {
          my $fc = parse_color($node->{'fillcolor'});
          my $redpct = POSIX::floor(100*$fc->{'red'}/255);
          my $greenpct = POSIX::floor(100*$fc->{'green'}/255);
          my $bluepct = POSIX::floor(100*$fc->{'blue'}/255);
          $fillstr = "red=$redpct% green=$greenpct% blue=$bluepct%\n";
        }

        my $point = transform_point({ 'x' => $bounds->{'left'}, 'y' => $bounds->{'bottom'}},
                                    $transform);
        my $height = $bounds->{'bottom'} - $bounds->{'top'};

        print ".$verb $nodename x=$point->{'x'} y=$point->{'y'} ".
              "scalex=$size->{'width'}% scaley=$size->{'height'}% $fillstr\n";

        print ".put ${nodename}___$stepno x=".($point->{'x'}+5).
          " y=".($point->{'y'}+$height/2)."\n";
      }
    }

    foreach my $edgename (keys(%$edges)) {
      print ".put ${edgename}___filled$stepno\n";
    }

    my $statuslabel = $dot->{'nodes'}->{'graph'}->{'label'};

    $frameno += 140;
    print ".frame $frameno\n";
    print ".action:\n";
#    print "title.htmlText=\"step $stepno, frame $frameno\";\n";
    print "title.htmlText=\"$statuslabel\";\n";
    print ".end\n";

    foreach my $nodename (keys(%$nodes)) {
      if (!ignore_node($nodename)) {
        print ".change $nodename\n";
      }
    }

    $frameno += 1;
    print ".frame $frameno\n";
    print ".action:\n";
    print "title.htmlText=\"$statuslabel\";\n";
    print ".end\n";

    @prevnodes = @thisnodes;
    $prevedges = $edges;
    $stepno++;
  }

  print ".end\n";


}

sub rect_size {
  my $rect = $_[0];

  return { 'width' => $rect->{'right'} - $rect->{'left'},
           'height' => $rect->{'bottom'} - $rect->{'top'} };
}

sub bounding_rect {
  my @rects = @_;

  my $left = 0;
  my $top = 0;
  my $right = 0;
  my $bottom = 0;

  my $first = 1;
  foreach my $rect (@rects) {
    if ($first || ($left > $rect->{'left'})) {
      $left = $rect->{'left'};
    }
    if ($first || ($top > $rect->{'top'})) {
      $top = $rect->{'top'};
    }
    if ($first || ($right < $rect->{'right'})) {
      $right = $rect->{'right'};
    }
    if ($first || ($bottom < $rect->{'bottom'})) {
      $bottom = $rect->{'bottom'};
    }
    $first = 0;
  }

  return { 'left' => $left, 'top' => $top, 'right' => $right, 'bottom' => $bottom };
}

sub annotate_dots {
  my @dots = @_;

  foreach my $dot (@dots) {
    annotate_dot($dot);
  }
}

sub dump_dots {
  my @dots = @_;

  foreach my $dot (@dots) {
    print "---------- $dot->{'filename'}: ".rect_str($dot->{'bounds'})."\n";
    dump_dot($dot);
  }
}

sub ignore_node {
  my $nodename = $_[0];

  return ($nodename eq "node" || $nodename eq "graph");
}

sub annotate_dot {
  my $dot = $_[0];
  my $nodes = $dot->{'nodes'};
  my $edges = $dot->{'edges'};
  my @noderects = ();

#  my $bbstr = $dot->{'nodes'}->{'graph'}->{'bb'} || die "$dot->{'filename'}: No bounding box";
#  $dot->{'bounds'} = parse_rect($bbstr);

  foreach my $node (values(%$nodes)) {
    if (!ignore_node($node->{'name'})) {
      my $rectstr = $node->{'rects'} || die "$dot->{'filename'}: node $node->{'name'} has no rects";
      my @rects = parse_rectlist($rectstr);
      $node->{'_rects'} = \@rects;

      $node->{'_bounds'} = bounding_rect(@rects);
      push(@noderects,$node->{'_bounds'});
    }
  }

  $dot->{'bounds'} = bounding_rect(@noderects);
  $dot->{'transform'} = { 'xoff' => -$dot->{'bounds'}->{'left'},
                          'yoff' => -$dot->{'bounds'}->{'top'},
                          'height' => $dot->{'bounds'}->{'bottom'} - $dot->{'bounds'}->{'top'} };

  return $dot;
}

sub parse_rectlist {
  my $str = $_[0];
  my @rects = ();

#  print "parse_rectlist: $str\n";
  my @rectstrs = split(/\s+/,$str);
  foreach my $rectstr (@rectstrs) {
    push(@rects,parse_rect($rectstr));
  }

  return @rects;
}

sub parse_rect {
  my $str = $_[0];

  $str =~ /^(\d+),(\d+),(\d+),(\d+)$/ || die "Invalid rect string: $str";
  return { 'left' => $1, 'top' => $2, 'right' => $3, 'bottom' => $4 };
}

sub rect_str {
  my $rect = $_[0];

  return "$rect->{'left'},$rect->{'top'},$rect->{'right'},$rect->{'bottom'}";
}

sub parse_point {
  my $str = $_[0];

  $str =~ /^(\d+),(\d+)$/ || die "Invalid point string: $str";
  return { 'x' => $1, 'y' => $2 };
}

sub point_str {
  my $point = $_[0];

  return "$point->{'x'},$point->{'y'}";
}

sub get_all_nodenames {
  my @dots = @_;
  my @nodenames = ();

  foreach $dot (@dots) {
    my $nodes = $dot->{'nodes'};
    foreach my $nodename (sort(keys(%$nodes))) {
      if (!grep(/^$nodename$/,@nodenames)) {
        push(@nodenames,$nodename);
      }
    }
  }

  return @nodenames;
}

sub get_all_edgenames {
  my @dots = @_;
  my @nodenames = ();

  foreach $dot (@dots) {
    my $edges = $dot->{'edges'};
    foreach my $edgename (sort(keys(%$edges))) {
      if (!grep(/^$edgename$/,@nodenames)) {
        push(@nodenames,$edgename);
      }
    }
  }

  return @nodenames;
}

sub read_file {
  my $filename = $_[0];
  my $contents = "";

  open(FILE,$filename) || die $filename;
  while (my $line = <FILE>) {
    $contents .= $line;
  }
  close(FILE);
  return $contents;
}


sub parse_params {
  my $str = $_[0];
  my $params = {};

  while ($str) {
    if ($str =~ /^\s*([a-zA-Z0-9_]+)\s*=
                  (\s*\"((\\\"|[^\"])*)\")\,?(.*)$/sx) {
      my $name = $1;
      my $value = $3;
      $str = $5;
      $params->{$name} = $value;
    }
    elsif ($str =~ /^\s*([a-zA-Z0-9_]+)\s*=
                  ([^\]\,]*)\,?(.*)$/sx) {
      my $name = $1;
      my $value = $2;
      $str = $3;
      $params->{$name} = $value;
    }
    else {
      die "Invalid param string: $str";
    }
  }

  return $params;
}

sub parse_dot_dir {

  my $dir = $_[0];
  my @dots = ();
  my $i = 0;

  opendir(DIR,$dir) || die $dir;
  my @entries = sort(grep(/step\d+\.dot$/,readdir(DIR)));
  closedir(DIR);

  foreach my $entry (@entries) {
    if (!-f "$dir/$entry.out") {
      system("dot $dir/$entry > $dir/$entry.out");
    }

    my $step = parse_dot(read_file("$dir/$entry.out"));
    $step->{'filename'} = $entry;

    push(@dots,$step);
  }

  return @dots;

}

sub parse_dot {
  my $dot = $_[0];
  my $vars = {};
  my $nodes = {};
  my $edges = {};

  $dot =~ /^\s*(digraph|graph)\s*\{(.*)\}\s*$/s || die "Invalid dot file";
  my $content = $2;

  while ($content) {
    if ($content =~ /^
                     \s*([a-zA-Z0-9_]+)\s*=
                     \s*\"((\\\"|[^\"])*)\"
                     \s*\;?(.*)$/sx) {

      my $name = $1;
      my $value = $2;
      $content = $4;
#      print "quoted: name :$name: value :$value: content :$content:\n";
      $vars->{$name} = $value;
    }
    elsif ($content =~ /^
                     \s*([a-zA-Z0-9_]+)\s*=
                     ([^\;]*)
                     \s*\;?(.*)$/sx) {

      my $name = $1;
      my $value = $2;
      $content = $3;
#      print "unquoted: name :$name: value :$value: content :$content:\n";
      $vars->{$name} = $value;
    }
    elsif ($content =~ /^\s*([a-zA-Z0-9_:]+)
                     (\s*\-\>\s*([a-zA-Z0-9_:]+))?
                     (\s*\[
                     ((\s*([a-zA-Z0-9_]+)\s*=
                      (\s*\"((\\\"|[^\"])*)\"|([^\]\,]*))\,?)*)
                     \])?\s*\;?(.*)$/sx) {
      my $nodename = $1;
      my $dest = $3;
      my $params = $5;

      my $props = parse_params($params);

      if ($dest) {
        my $from = $nodename;
        my $to = $dest;
        $from =~ s/:.*$//;
        $to =~ s/:.*$//;
        my $name = $from."___".$to;
        $props->{'name'} = $name;
        $props->{'from'} = $nodename;
        $props->{'to'} = $dest;
#        print "nodename :$nodename: dest :$dest:\n";
#        $edges->{$name} = $props;
        foreach my $propname (keys(%$props)) {
          $edges->{$name}->{$propname} = $props->{$propname};
        }
      }
      else {
        $props->{'name'} = $nodename;
#        $nodes->{$nodename} = $props;
        foreach my $propname (keys(%$props)) {
          $nodes->{$nodename}->{$propname} = $props->{$propname};
        }
      }

      $content = $12;
    }
    elsif ($content =~ /^\s*$/) {
      $content = "";
    }
    else {
      die "Unknown line format: $content\n";
    }
  }

  return { 'vars' => $vars, 'nodes' => $nodes, 'edges' => $edges };
}

sub dump_dot {
  my $dot = $_[0];
  my $vars = $dot->{'vars'};
  my $nodes = $dot->{'nodes'};
  my $edges = $dot->{'edges'};

  foreach my $name (keys %$vars) {
    print "Var $name: $vars->{$name}\n";
  }
  print "\n";

  foreach my $nodename (keys %$nodes) {
    print "Node $nodename\n";
    my $params = $nodes->{$nodename};
    foreach my $name (keys %$params) {
      print "  $name=$params->{$name}\n";
    }
    print "\n";
  }

  foreach my $edgename (keys %$edges) {
    print "Edge $edgename\n";
    my $params = $edges->{$edgename};
    foreach my $name (keys %$params) {
      print "  $name=$params->{$name}\n";
    }
    print "\n";
  }

}
