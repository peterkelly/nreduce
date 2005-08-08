noinst_LTLIBRARIES = libdataflow.la
noinst_PROGRAMS = 
BUILT_SOURCES = funops.c
CLEANFILES = funops.c funops.h
libdataflow_la_SOURCES = dataflow.c sequencetype.c funops.c engine.c optimization.c validity.c
gridxsltincludedir = $(includedir)/gridxslt
noinst_HEADERS = dataflow.h sequencetype.h funops.h engine.h optimization.h validity.h
INCLUDES = -I@top_srcdir@ @ALL_INCLUDES@
LDADD = libdataflow.la ../xmlschema/libxmlschema.la ../util/libutil.la @ALL_LIBS@

funops.c:
	perl funops.pl
