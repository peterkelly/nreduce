import service namespace store = "http://localhost:8080/store?WSDL";

let $products := store:getIds()/item/text()/store:getProduct(.)
return
for $category in store:getCategories()/item/text()
return
<category name="{$category}">{
  $products[category eq $category]
}</category>
