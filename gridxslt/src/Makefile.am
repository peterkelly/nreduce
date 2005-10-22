INCLUDES = -I@top_srcdir@ @ALL_INCLUDES@
LDADD = ../libgridxslt.la libsrc.la @ALL_LIBS@
noinst_PROGRAMS = gridxslt runtests cbpel validate wsclient fsxml saxonclient buffertest transfer client
noinst_HEADERS = syntax_bpel.h fsm.h syntax_wsdl.h
gridxslt_SOURCES = gridxslt.c
runtests_SOURCES = runtests.c
cbpel_SOURCES = cbpel.c
validate_SOURCES = validate.c
wsclient_SOURCES = wsclient.c
fsxml_SOURCES = fsxml.c
saxonclient_SOURCES = saxonclient.c
buffertest_SOURCES = buffertest.c
transfer_SOURCES = transfer.c
client_SOURCES = client.c

noinst_LTLIBRARIES = libsrc.la
libsrc_la_SOURCES = syntax_bpel.c fsm.c syntax_wsdl.c gridxslt_impl.c

