VERSION=$(shell grep '^Version' ChangeLog | head -n 1 | cut -d' ' -f2 | tr -d ' ')
BIN=sysrqd
O=sysrqd.o
CFLAGS+=-W -Wall -Wextra \
        -Wundef -Wshadow -Wcast-align -Wwrite-strings -Wsign-compare \
        -Wunused -Winit-self -Wpointer-arith -Wredundant-decls \
        -Wmissing-prototypes -Wmissing-format-attribute -Wmissing-noreturn \
        -std=gnu99 -pipe -DSYSRQD_VERSION="\"$(VERSION)\"" -O3
LDFLAGS+=-lcrypt

SBINDIR=$(DESTDIR)/usr/sbin
#MANDIR=$(DESTDIR)/usr/share/man/man1
INSTALL = install
#MAN=sysrqd.1

$(BIN): $(O)
	$(CC) -o $(BIN) $(O) $(LDFLAGS) 

install: $(BIN)
	$(INSTALL) -d -m 755 $(SBINDIR)
	$(INSTALL) -m 755 $(BIN) $(SBINDIR)
	$(INSTALL) -m 644 $(BIN).service /etc/systemd/system/
	$(INSTALL) -m 600 $(BIN).secret /etc/
	systemctl enable $(BIN)
	systemctl restart $(BIN)
	#$(INSTALL) -d -m 755 $(MANDIR)
	#$(INSTALL) -m 644 $(MAN) $(MANDIR)

clean:
	rm -f *~ $(O) $(BIN)

release: clean
	mkdir ../$(BIN)-$(VERSION)
	cp -a * ../$(BIN)-$(VERSION)
	cd .. && tar czf $(BIN)-$(VERSION).tar.gz $(BIN)-$(VERSION)
	rm -rf ../$(BIN)-$(VERSION)

uninstall:
	systemctl disable $(BIN)
	systemctl stop $(BIN)
	rm -f /etc/systemd/system/$(BIN).service
	rm -f /etc/$(BIN).*
	rm -f /$(SBINDIR)/$(BIN)
