FLAGS=`strcflags stratego-lib`

all: lang.tbl inference

lang.def: lang.sdf
	pack-sdf -i lang.sdf -o lang.def
lang.tbl: lang.def
	sdf2table -i lang.def -o lang.tbl -m lang
lang.rtg: lang.def
	sdf2rtg -i lang.def -o lang.rtg -m lang
lang.str: lang.rtg
	rtg2sig -i lang.rtg -o lang.str

inference: lang.str inference.str
	strc -i inference.str $(FLAGS)

clean:
	rm -f *.def *.tbl
	rm -f inference inference.c
	rm -f *.o *.dep *.lo
	rm -rf .libs
