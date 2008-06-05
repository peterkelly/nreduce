import service namespace people = "http://localhost:8081/peoplerpc?WSDL";

let $list :=
  for $name in people:getNames()/item/text()
  return <person>{people:getPerson($name)/*}</person>
return
<people totalAges="{people:totalAges($list/<item>{*}</item>)}">{
  $list
}</people>
