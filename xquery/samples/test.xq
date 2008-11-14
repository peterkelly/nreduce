import service namespace test = "http://localhost:8080/test?WSDL";

let $figures :=
      <sales-figures>
      {for $loc in test:getLocations()/item/node()
       let $items := test:getSalesData($loc)/item
       return
        <location name="{$loc}" sales="{count($items)}" revenue="{sum($items/amount)}"/>}
      </sales-figures>,

    $nlocations := count($figures/location),
    $maxsales := max($figures/location/@sales),

    $top := 50,
    $left := 150,
    $height := 20 * $nlocations,
    $width := 10 * $maxsales

return

    <s:svg width="100%" height="100%" version="1.1"
           xmlns:s="http://www.w3.org/2000/svg">

      <s:text x="300" y="30" font-size="24">Sales by location</s:text>

      <s:line x1="{$left - 4}"
              y1="{$top}"
              x2="{$left - 4}"
              y2="{$top+$height}"
              stroke="black"
              stroke-width="2"/>

      <s:line x1="{$left - 4}"
              y1="{$top+$height}"
              x2="{$left+$width}"
              y2="{$top+$height}"
              stroke="black"
              stroke-width="2"/>

      {for $row at $pos in $figures/location
       let $width := $row/@sales * 10
       return (<s:rect x="{$left}"
                       y="{$top + 20 * ($pos - 1)}"
                       width="{$width}"
                       height="15"
                       fill="blue"/>,
               <s:text x="20"
                       y="{$top + 20 * ($pos - 1) + 15}">
                 {string($row/@name)}
               </s:text>)}

      {for $x2 in (0 to xs:integer($maxsales div 5))
       let $x := $x2 * 5
       return (<s:line x1="{$left + 10*$x}"
                       y1="{$top + $height}"
                       x2="{$left + 10*$x}"
                       y2="{$top + $height + 10}"
                       stroke="black"
                       stroke-width="2"/>,
               <s:text x="{$left + 10*$x}"
                       y="{$top + $height + 30}"
                       text-anchor="middle">
                 {$x}
               </s:text>)}
    </s:svg>
