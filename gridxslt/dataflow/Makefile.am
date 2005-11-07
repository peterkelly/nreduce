noinst_LTLIBRARIES = libdataflow.la
noinst_PROGRAMS = 
libdataflow_la_SOURCES = Engine.cpp Optimization.cpp Validity.cpp Functions.cpp Serialization.cpp Program.cpp SequenceType.cpp
gridxsltincludedir = $(includedir)/gridxslt
noinst_HEADERS = Engine.h Optimization.h Validity.h Serialization.h Program.h SequenceType.h
INCLUDES = -I@top_srcdir@ @ALL_INCLUDES@
LDADD = libdataflow.la ../xmlschema/libxmlschema.la ../util/libutil.la @ALL_LIBS@
CLEANFILES = *.da *.bb *.bbg *.gcov
