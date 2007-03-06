INCLUDES = -I@top_srcdir@ @ALL_INCLUDES@
LDADD = ../libgridxslt.la libsrc.la @ALL_LIBS@
#noinst_PROGRAMS = gridxslt runtests fsxml saxonclient buffertest transfer client test binencode
noinst_PROGRAMS = gridxslt runtests fsxml
noinst_HEADERS = 
gridxslt_SOURCES = gridxslt.cpp
#gridxslt_LDFLAGS = -static
gridxslt_LDADD = ../libgridxslt.la gridxslt_impl.o @ALL_LIBS@
runtests_SOURCES = runtests.cpp
#runtests_LDFLAGS = -static
runtests_LDADD = ../libgridxslt.la gridxslt_impl.o @ALL_LIBS@

fsxml_SOURCES = fsxml.cpp
#saxonclient_SOURCES = saxonclient.cpp
#buffertest_SOURCES = buffertest.cpp
#transfer_SOURCES = transfer.cpp
#client_SOURCES = client.cpp
#test_SOURCES = test.cpp
#test_LDFLAGS = -static
#test_LDADD = ../libgridxslt.la gridxslt_impl.o @ALL_LIBS@
#binencode_SOURCES = binencode.cpp
#binencode_LDFLAGS = -static
#binencode_LDADD = ../libgridxslt.la gridxslt_impl.o @ALL_LIBS@

noinst_LTLIBRARIES = libsrc.la
libsrc_la_SOURCES = gridxslt_impl.cpp

CLEANFILES = *.da *.bb *.bbg *.gcov
