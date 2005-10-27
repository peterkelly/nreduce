INCLUDES = -I@top_srcdir@ @ALL_INCLUDES@
LDADD = ../libgridxslt.la libsrc.la @ALL_LIBS@
noinst_PROGRAMS = gridxslt runtests validate wsclient fsxml saxonclient buffertest transfer client test
noinst_HEADERS = fsm.h syntax_wsdl.h
gridxslt_SOURCES = gridxslt.c
gridxslt_LDFLAGS = -static
gridxslt_LDADD = ../libgridxslt.la gridxslt_impl.o @ALL_LIBS@
runtests_SOURCES = runtests.c
runtests_LDFLAGS = -static
runtests_LDADD = ../libgridxslt.la gridxslt_impl.o @ALL_LIBS@
validate_SOURCES = validate.c
wsclient_SOURCES = wsclient.c
fsxml_SOURCES = fsxml.c
saxonclient_SOURCES = saxonclient.c
buffertest_SOURCES = buffertest.c
transfer_SOURCES = transfer.c
client_SOURCES = client.c
test_SOURCES = test.cpp
test_LDFLAGS = -static
test_LDADD = ../libgridxslt.la gridxslt_impl.o @ALL_LIBS@

noinst_LTLIBRARIES = libsrc.la
libsrc_la_SOURCES = fsm.c syntax_wsdl.c gridxslt_impl.c


