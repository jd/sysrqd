VERSION=1
O=sysrqd.o
CFLAGS=-W -Wall
BIN=sysrqd

$(BIN): $(O)
	$(CC) -o $(BIN) $(LDFLAGS) $(O)

clean:
	rm -f $(BIN) $(O)

distclean: clean
	rm -f *~
