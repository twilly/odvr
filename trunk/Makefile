CC=gcc
CFLAGS=-g -O2 -Wall
LDFLAGS=
LIBS=-lusb -lsndfile -lm
BINS=odvr odvr.x86 odvr-gui
TICONS=TBA TBB TBC TBD TBS TBT
FICONS=FCA FCB FCC FCD FCS
ICONS=$(TICONS) $(FICONS)
X86LIBS=-L$(HOME)/build/lib32
PREFIX=/usr/local
SYSCONFDIR=/etc
VERSION=0.1.4.1-cml
GUICFLAGS  = `pkg-config gtk+-2.0 --cflags`
LDADD   = `pkg-config gtk+-2.0 --libs`
CFLAGS+=$(GUICFLAGS)

all: odvr odvr-gui

install: odvr odvr-gui
	install -o root -g root -m 755 odvr $(PREFIX)/bin
	install -o root -g root -m 755 odvr-gui $(PREFIX)/bin
	-lsb_release -a 2>/dev/null | grep -q Ubuntu && install -o root -g root -m 644 \
		41-odvr.rules $(SYSCONFDIR)/udev/rules.d/ && /etc/init.d/udev reload

release: $(BINS)
	-rm -rf debpkg/usr debpkg/etc
	mkdir -p debpkg/usr/bin debpkg/etc/udev/rules.d
	cp odvr debpkg/usr/bin
	cp 41-odvr.rules debpkg/etc/udev/rules.d
	dpkg -b debpkg odvr-$(VERSION).deb

odvr: cli.o olympusdvr.o
	$(CC) $(CFLAGS) $(LDFLAGS)-o $@ $^ $(LIBS)

odvr.x86: cli.c olympusdvr.c
	gcc -static $(X86LIBS) -m32 -O2 -Wall -o $@ $^ -lusb -lsndfile -lm
	strip $@

odvr_icons.h:
	@ echo Making $@ 
	@ gdk-pixbuf-csource --struct --extern --build-list `echo $(ICONS) | \
	awk '{ for (i = 1; i <= NF; i++) printf("%s icons/%s.png  ", $$i, $$i) }'` | \
	sed 's/\/\* pixel_data: \*\// \/\* pixel_data \*\/ (guint8 *)/'  > $@

odvr_icons.c: odvr_icons.h


odvr-gui: odvr_icons.o gui.o odvr_gui.o odvr_date.o odvr_cfg.o olympusdvr.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS) $(LDADD)

clean:
	-rm -f *.o *~ \#*\# $(BINS) *.deb odvr_icons.h
