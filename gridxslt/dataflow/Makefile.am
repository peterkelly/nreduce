noinst_LTLIBRARIES = libdataflow.la
noinst_PROGRAMS = 
libdataflow_la_SOURCES = dataflow.c sequencetype.c engine.c optimization.c validity.c functions.c serialization.c
gridxsltincludedir = $(includedir)/gridxslt
noinst_HEADERS = dataflow.h sequencetype.h engine.h optimization.h validity.h serialization.h
INCLUDES = -I@top_srcdir@ @ALL_INCLUDES@
LDADD = libdataflow.la ../xmlschema/libxmlschema.la ../util/libutil.la @ALL_LIBS@

