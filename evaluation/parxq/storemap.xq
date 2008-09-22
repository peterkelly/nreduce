import service namespace store = "http://localhost:8080/store?WSDL";

<products>{
for $id in store:getIds()/item/text()
return store:getProduct($id)
}</products>
