INCLUDES = -I@top_srcdir@ @XML_CPPFLAGS@ @LIBCURL_CPPFLAGS@
LDADD = ../libgridxslt.la libsrc.la @ALL_LIBS@
noinst_PROGRAMS = gridxslt runtests cbpel validate wsclient fsxml saxonclient
noinst_HEADERS = syntax_bpel.h fsm.h syntax_wsdl.h
gridxslt_SOURCES = gridxslt.c
gridxslt_LDFLAGS = -static
runtests_SOURCES = runtests.c
runtests_LDFLAGS = -static
cbpel_SOURCES = cbpel.c
validate_SOURCES = validate.c
wsclient_SOURCES = wsclient.c
fsxml_SOURCES = fsxml.c
saxonclient_SOURCES = saxonclient.c
saxonclient_LDFLAGS = -static

noinst_LTLIBRARIES = libsrc.la
libsrc_la_SOURCES = syntax_bpel.c fsm.c syntax_wsdl.c

