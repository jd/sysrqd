VERSION=$(shell grep '^Version' ChangeLog | head -n 1 | cut -d' ' -f2 | tr -d ' ')
BIN=sysrqd
O=sysrqd.o
LDFLAGS=
CFLAGS=-W -Wall -DSYSRQD_VERSION="\"$(VERSION)\"" -g

SBINDIR=$(DESTDIR)/usr/sbin
#MANDIR=$(DESTDIR)/usr/share/man/man1
INSTALL = install
#MAN=sysrqd.1

$(BIN): $(O)
	$(CC) $(LDFLAGS) -o $(BIN) $(O)

install: $(BIN)
	$(INSTALL) -d -m 755 $(SBINDIR)
	$(INSTALL) -m 755 $(BIN) $(SBINDIR)

	#$(INSTALL) -d -m 755 $(MANDIR)
	#$(INSTALL) -m 644 $(MAN) $(MANDIR)

clean:
	rm -f *~ $(O) $(BIN)

release: clean
	mkdir ../$(BIN)-$(VERSION)
	cp -a * ../$(BIN)-$(VERSION)
	cd .. && tar czf $(BIN)-$(VERSION).tar.gz $(BIN)-$(VERSION)
	rm -rf ../$(BIN)-$(VERSION)
