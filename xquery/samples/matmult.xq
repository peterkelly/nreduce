import service namespace test = "http://localhost:8080/test?WSDL";

let $a := test:getMatrix(4,4,0)/item,
    $b := test:getMatrix(4,4,1)/item
return test:addMatrices($a,$b)
