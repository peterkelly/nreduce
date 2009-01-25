all:
	javac *.java
	wsgen -cp .:$(CLASSPATH) -wsdl Empty
	wsgen -cp .:$(CLASSPATH) -wsdl Hello
	wsgen -cp .:$(CLASSPATH) -wsdl Compute
	wsgen -cp .:$(CLASSPATH) -wsdl Marks
	wsimport -keep -p proxy.hello HelloService.wsdl
	wsimport -keep -p proxy.compute ComputeService.wsdl
	wsimport -keep -p proxy.marks MarksService.wsdl
	javac client/*.java

clean:
	rm -f *.class client/*.class *.wsdl *.xsd
	rm -rf proxy
