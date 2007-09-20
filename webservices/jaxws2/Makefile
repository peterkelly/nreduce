SERVICE=test2
MAIN_CLASS=Test2

all:
	javac $(SERVICE)/*.java
	wsgen -cp . -wsdl $(SERVICE).$(MAIN_CLASS)
	wsimport -p proxy *Service.wsdl
	javac client/*.java

clean:
	rm -f *.wsdl *.xsd $(SERVICE)/*.class client/*.class
	rm -rf $(SERVICE)/jaxws proxy
