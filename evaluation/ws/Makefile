all:
	javac *.java
	wsgen -cp .:$(CLASSPATH) -wsdl Empty
	wsgen -cp .:$(CLASSPATH) -wsdl Hello
	wsgen -cp .:$(CLASSPATH) -wsdl Compute
	wsgen -cp .:$(CLASSPATH) -wsdl Marks
	wsgen -cp .:$(CLASSPATH) -wsdl People
	wsimport -keep -p proxy.hello HelloService.wsdl
	wsimport -keep -p proxy.compute ComputeService.wsdl
	wsimport -keep -p proxy.marks MarksService.wsdl
	wsimport -keep -p proxy.marks PeopleService.wsdl
	javac client/*.java

clean:
	rm -f *.class client/*.class *.wsdl *.xsd
	rm -rf proxy
