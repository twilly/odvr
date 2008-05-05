CC=gcc
CFLAGS=-g -O2 -Wall
LDFLAGS=
LIBS=-lusb -lsndfile -lm
BINS=odvr odvr.x86
X86LIBS=-L$(HOME)/build/lib32
PREFIX=/usr/local
SYSCONFDIR=/etc
VERSION=0.1.4.1

all: odvr

install: odvr
	install -o root -g root -m 755 odvr $(PREFIX)/bin
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

clean:
	-rm -f *.o *~ \#*\# $(BINS) *.deb
