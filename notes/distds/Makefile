EPSFILES=$(patsubst %.fig,%.eps,$(wildcard figures/*/*.fig))

all: distds.pdf

distds.pdf: distds.tex ../styles/PPRnotes.sty $(EPSFILES)
	latex distds
	dvipdf distds

%.eps: %.fig
	fig2dev -L eps $< $@

clean:
	rm -f *.aux *.dvi *.log *.pdf *.out figures/*/*.eps
