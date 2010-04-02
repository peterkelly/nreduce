import service namespace forestry = "http://localhost:5000/forestry?WSDL";

declare variable $steps := 32;
declare variable $minlat := 5.26;
declare variable $maxlat := 8.82;
declare variable $minlong := 85.38;
declare variable $maxlong := 88.94;
declare variable $latmult := ($maxlat - $minlat) div $steps;
declare variable $longmult := ($maxlong - $minlong) div $steps;

declare variable $width := 1000;
declare variable $height := 400;
declare variable $cellwidth := $width div (2*$steps);
declare variable $cellheight := $height div (2*$steps);

declare function local:position($row,$col,$value)
{
  ($cellwidth*(1 - $row + $col) + $width div 2,
  ",",
  $cellheight*($row + $col - 1) - floor($value))
};

let $data :=
  for $y in 1 to $steps
  let $lat := $minlat + $y * $latmult
  return
  <row lat="{$lat}">{
    for $x in 1 to $steps
    let $long := $minlong + $x * $longmult
    return
    <col long="{$long}">{
      forestry:analyze($lat,$long,$latmult,$longmult,2004) -
      forestry:analyze($lat,$long,$latmult,$longmult,2009)
    }</col>
  }</row>

return
<svg version="1.1" xmlns="http://www.w3.org/2000/svg">{
for $diag in 1 to $steps * 2 - 1,
    $horiz in max((2,$diag+1-$steps)) to min(($steps,$diag - 1))
let $row := $diag + 1 - $horiz,
    $col := $horiz,
    $colour := floor(155+$data[$row]/*[$col])
return 
<polygon
  points="
  {local:position($row - 1,$col - 1,$data[$row - 1]/*[$col - 1])}
  {local:position($row - 1,$col,    $data[$row - 1]/*[$col])}
  {local:position($row,    $col,    $data[$row]    /*[$col])}
  {local:position($row,    $col - 1,$data[$row]    /*[$col - 1])}"
  style="fill:rgb({$colour},{$colour},{$colour});
         stroke:black;stroke-width:0.5"/>
}</svg>
