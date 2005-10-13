noinst_LTLIBRARIES = libhttp.la
noinst_PROGRAMS = 
libhttp_la_SOURCES = http.c message.c handlers.c
gridxsltincludedir = $(includedir)/gridxslt
noinst_HEADERS = http.h message.h handlers.h
INCLUDES = -I@top_srcdir@ @ALL_INCLUDES@
LDADD = libhttp.la ../util/libutil.la @ALL_LIBS@
