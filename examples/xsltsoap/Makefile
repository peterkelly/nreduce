all: servlet calcclient

servlet:
	javac -d WEB-INF/classes xslt/*.java

calcclient:
	java org.apache.axis.wsdl.WSDL2Java -s -p client.stubs calculator.wsdl
	javac client/*.java client/stubs/*.java

clean:
	rm -rf WEB-INF/classes/xslt client/stubs client/*.class
