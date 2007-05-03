EPSFILES=$(patsubst %.fig,%.eps,$(wildcard figures/*/*.fig))

all: heap.pdf

heap.pdf: heap.tex ../styles/PPRnotes.sty $(EPSFILES)
	latex heap
	dvipdf heap

%.eps: %.fig
	fig2dev -L eps $< $@

clean:
	rm -f *.aux *.dvi *.log *.pdf *.out figures/*/*.eps
