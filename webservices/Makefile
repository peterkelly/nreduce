all:
	javac service/*.java service/*/*.java
	wsgen -cp . -wsdl service.hellorpc.HelloRPC
	wsgen -cp . -wsdl service.hellodw.HelloDW
	wsgen -cp . -wsdl service.calcrpc.CalcRPC
	wsgen -cp . -wsdl service.calcdw.CalcDW
	wsgen -cp . -wsdl service.peoplerpc.PeopleRPC
	wsgen -cp . -wsdl service.peopledw.PeopleDW
	wsimport -p proxy.hellorpc HelloRPCService.wsdl
	wsimport -p proxy.hellodw HelloDWService.wsdl
	wsimport -p proxy.calcrpc CalcRPCService.wsdl
	wsimport -p proxy.calcdw CalcDWService.wsdl
	wsimport -p proxy.peoplerpc PeopleRPCService.wsdl
	wsimport -p proxy.peopledw PeopleDWService.wsdl
	javac client/*.java

clean:
	rm -f *.wsdl *.xsd service/*.class service/*/*.class client/*.class
	rm -rf service/*/jaxws proxy
