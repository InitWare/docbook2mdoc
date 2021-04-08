VERSION = 1.1.0
CFLAGS += -g -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings
WWWPREFIX = /var/www/vhosts/mdocml.bsd.lv/htdocs/docbook2mdoc
PREFIX = /usr/local

HEADS =	xmalloc.h node.h parse.h reorg.h macro.h format.h
SRCS =	xmalloc.c node.c parse.c reorg.c macro.c docbook2mdoc.c tree.c main.c
OBJS =	xmalloc.o node.o parse.o reorg.o macro.o docbook2mdoc.o tree.o main.o
DISTFILES = Makefile NEWS docbook2mdoc.1

all: docbook2mdoc

docbook2mdoc: $(OBJS)
	$(CC) -g -o $@ $(OBJS)

statistics: statistics.o xmalloc.o
	$(CC) -g -o $@ statistics.o xmalloc.o

www: docbook2mdoc.1.html docbook2mdoc-$(VERSION).tgz README.txt

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/man/man1
	install -m 0755 docbook2mdoc $(DESTDIR)$(PREFIX)/bin
	install -m 0444 docbook2mdoc.1 $(DESTDIR)$(PREFIX)/man/man1

installwww: www
	mkdir -p $(WWWPREFIX)/snapshots
	install -m 0444 docbook2mdoc.1.html README.txt NEWS $(WWWPREFIX)
	install -m 0444 docbook2mdoc-$(VERSION).tgz $(WWWPREFIX)/snapshots
	ln -f $(WWWPREFIX)/snapshots/docbook2mdoc-$(VERSION).tgz \
	    $(WWWPREFIX)/snapshots/docbook2mdoc.tgz

dist: docbook2mdoc-$(VERSION).tgz

docbook2mdoc-$(VERSION).tgz:
	mkdir -p .dist/docbook2mdoc-$(VERSION)
	install -m 0444 $(HEADS) $(SRCS) $(DISTFILES) \
	    .dist/docbook2mdoc-$(VERSION)
	(cd .dist && tar zcf ../$@ docbook2mdoc-$(VERSION))
	rm -rf .dist

xmalloc.o: xmalloc.h
node.o: xmalloc.h node.h
parse.o: xmalloc.h node.h parse.h
reorg.o: node.h reorg.h
macro.o: node.h macro.h
docbook2mdoc.o: xmalloc.h node.h macro.h format.h
tree.o: node.h format.h
main.o: node.h parse.h format.h
statistics.c: xmalloc.h

docbook2mdoc.1.html: docbook2mdoc.1
	mandoc -T html -O style=/mandoc.css docbook2mdoc.1 >$@

README.txt: README
	cp -p README $@

clean:
	rm -f docbook2mdoc $(OBJS) docbook2mdoc.core
	rm -f statistics statistics.o statistics.core
	rm -rf docbook2mdoc.dSYM
	rm -f index.html docbook2mdoc.1.html README.txt
	rm -f docbook2mdoc-$(VERSION).tgz
