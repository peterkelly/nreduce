import service namespace test1 = "http://titan01:8080/test?WSDL";
import service namespace test2 = "http://titan02:8080/test?WSDL";
import service namespace test3 = "http://titan03:8080/test?WSDL";
import service namespace test4 = "http://titan04:8080/test?WSDL";

let $initial := "ABCDE",
    $res1 := test1:addChar($initial)/text(),
    $res2 := test2:addChar($res1)/text(),
    $res3 := test3:addChar($res2)/text(),
    $res4 := test4:addChar($res3)/text()
return
    $res4
