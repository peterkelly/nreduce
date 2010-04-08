import service namespace comp = "http://localhost:5000/compute?WSDL";

declare variable $size := 32;
declare variable $compms := 32;

for $i in 1 to $size
return comp:compute($i,$compms)
