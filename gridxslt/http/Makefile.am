noinst_LTLIBRARIES = libhttp.la
noinst_PROGRAMS = 
libhttp_la_SOURCES = HTTP.cpp Message.cpp Handlers.cpp
gridxsltincludedir = $(includedir)/gridxslt
noinst_HEADERS = HTTP.h Message.h Handlers.h
INCLUDES = -I@top_srcdir@ @ALL_INCLUDES@
LDADD = libhttp.la ../util/libutil.la @ALL_LIBS@
CLEANFILES = *.da *.bb *.bbg *.gcov
