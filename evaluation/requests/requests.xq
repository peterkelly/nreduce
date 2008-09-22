import service namespace hello = "http://localhost:8080/hellorpc?WSDL";

count(for $i in (1 to 1000)
return hello:sayHello("Peter"))
