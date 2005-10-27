noinst_LTLIBRARIES = libdataflow.la
noinst_PROGRAMS = 
libdataflow_la_SOURCES = engine.c optimization.c validity.c functions.c serialization.c Program.cpp SequenceType.cpp
gridxsltincludedir = $(includedir)/gridxslt
noinst_HEADERS = engine.h optimization.h validity.h serialization.h Program.h SequenceType.h
INCLUDES = -I@top_srcdir@ @ALL_INCLUDES@
LDADD = libdataflow.la ../xmlschema/libxmlschema.la ../util/libutil.la @ALL_LIBS@

