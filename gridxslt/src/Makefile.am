INCLUDES = -I@top_srcdir@ @XML_CPPFLAGS@ @LIBCURL_CPPFLAGS@
LDADD = ../libgridxslt.la libsrc.la @ALL_LIBS@
noinst_PROGRAMS = gridxslt runtests cbpel create matrix validate wsclient wsjava asm fsxml
noinst_HEADERS = syntax_bpel.h fsm.h syntax_wsdl.h
gridxslt_SOURCES = gridxslt.c
runtests_SOURCES = runtests.c
cbpel_SOURCES = cbpel.c
create_SOURCES = create.c
matrix_SOURCES = matrix.c
validate_SOURCES = validate.c
wsclient_SOURCES = wsclient.c
wsjava_SOURCES = wsjava.c
asm_SOURCES = asm.c
fsxml_SOURCES = fsxml.c

noinst_LTLIBRARIES = libsrc.la
libsrc_la_SOURCES = syntax_bpel.c fsm.c syntax_wsdl.c

