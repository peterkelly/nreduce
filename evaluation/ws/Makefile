all:
	javac *.java
	wsgen -cp . -wsdl Empty
	wsgen -cp . -wsdl Hello
	wsgen -cp . -wsdl Compute
	wsimport -p proxy.hello HelloService.wsdl
	wsimport -p proxy.compute ComputeService.wsdl
	javac client/*.java

clean:
	rm -f *.class client/*.class *.wsdl
	rm -rf proxy
