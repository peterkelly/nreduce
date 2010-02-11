import service namespace hr = "http://localhost:8080/hr?WSDL";

<results>{
for $id in hr:listEmployees()/text()
let $info := hr:employeeInfo($id)
return <employee id="{$id}"
                 name="{$info/self::firstName,' ',$info/self::lastName}"
                 salary="{$info/self::salary}"/>
}</results>
