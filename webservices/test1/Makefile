SERVICE=test1
MAIN_CLASS=Test1

all:
	javac $(SERVICE)/*.java
	java org.apache.axis.wsdl.Java2WSDL \
	-o $(SERVICE).wsdl \
	-l "http://localhost:8080/axis/services/$(SERVICE)" \
	-P $(MAIN_CLASS) $(SERVICE).$(MAIN_CLASS) \
	-u LITERAL \
	-y WRAPPED
	java org.apache.axis.wsdl.WSDL2Java -o . -d Session -s -p ws $(SERVICE).wsdl
	javac ws/*.java client/*.java
	cat ws/deploy.wsdd | sed -e 's/ws\.$(MAIN_CLASS)SoapBindingImpl/$(SERVICE).$(MAIN_CLASS)/' > deploy.wsdd
	mv -f deploy.wsdd ws/deploy.wsdd
	jar cf $(SERVICE).jar $(SERVICE)/$(MAIN_CLASS).class

deploy:
	cp $(SERVICE).jar ${CATALINA_HOME}/webapps/axis/WEB-INF/lib
	java org.apache.axis.client.AdminClient ws/deploy.wsdd

undeploy:
	java org.apache.axis.client.AdminClient ws/undeploy.wsdd
	rm -f ${CATALINA_HOME}/webapps/axis/WEB-INF/lib/$(SERVICE).jar

clean:
	rm -rf ws
	rm -f $(SERVICE).wsdl $(SERVICE).jar $(SERVICE)/*.class client/*.class
