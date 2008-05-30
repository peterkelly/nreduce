FLAGS=`strcflags stratego-lib`

all: xquery.tbl elc.tbl xquery.pp elc.pp xquery-parenthesize elc-parenthesize compile nopriorities

compile: compile.str concrete.tbl
	strc -i compile.str $(FLAGS)

nopriorities: nopriorities.str
	strc -i nopriorities.str $(FLAGS)

xquery.def: xquery.sdf
	pack-sdf -i xquery.sdf -o xquery.def
xquery.tbl: xquery.def
	sdf2table -i xquery.def -o xquery.tbl -m xquery
xquery.pp: xquery.def nopriorities
	parse-sdf-definition -i xquery.def | ./nopriorities | pp-sdf -o xquery-fixed.def
	ppgen -i xquery-fixed.def -o xquery.pp
xquery.rtg: xquery.def
	sdf2rtg -i xquery.def -o xquery.rtg -m xquery
xquery.str: xquery.rtg
	rtg2sig -i xquery.rtg -o xquery.str
xquery-parenthesize.str: xquery.def xquery.str
	sdf2parenthesize -i xquery.def -o xquery-parenthesize.str -m xquery --lang xquery
xquery-parenthesize: xquery-parenthesize.str
	strc -i xquery-parenthesize.str -m io-xquery-parenthesize -la stratego-lib

elc.def: elc.sdf
	pack-sdf -i elc.sdf -o elc.def
elc.tbl: elc.def
	sdf2table -i elc.def -o elc.tbl -m elc
elc.pp: elc.def nopriorities
	ppgen -i elc.def -o elc.pp
elc.rtg: elc.def
	sdf2rtg -i elc.def -o elc.rtg -m elc
elc.str: elc.rtg
	rtg2sig -i elc.rtg -o elc.str
elc-parenthesize.str: elc.def elc.str
	sdf2parenthesize -i elc.def -o elc-parenthesize.str -m elc --lang elc
elc-parenthesize: elc-parenthesize.str
	strc -i elc-parenthesize.str -m io-elc-parenthesize -la stratego-lib

concrete.def: concrete.sdf xquery.def elc.def
	pack-sdf -i concrete.sdf -o concrete.def
concrete.tbl: concrete.def
	sdf2table -i concrete.def -o concrete.tbl -m concrete

clean:
	rm -f xquery.def  xquery.tbl
	rm -f xquery-fixed.def xquery.pp xquery.rtg xquery.str
	rm -f xquery-parenthesize.str xquery-parenthesize.c xquery-parenthesize
	rm -f elc.def  elc.tbl
	rm -f elc.pp elc.rtg elc.str
	rm -f elc-parenthesize.str elc-parenthesize.c elc-parenthesize
	rm -f concrete.def concrete.tbl
	rm -f *.dep *.lo *.o
	rm -rf .libs
	rm -f compile.c compile
	rm -f nopriorities.c nopriorities
