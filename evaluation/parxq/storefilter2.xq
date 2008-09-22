import service namespace store = "http://localhost:8080/store?WSDL";

for $category in store:getCategories()/item/text()
return
<category name="{$category}">{
  for $id in (store:getIds()/item/text())[store:getProduct(.)/category eq $category]
  return store:getProduct($id)
}</category>
