PREFIX = /usr/local

CFLAGS = -I/usr/local/include
LDFLAGS = -L/usr/local/lib
LDLIBS = -lm -lcurses -lfftw3

BIN = spectrum
all: $(BIN)

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)

clean:
	rm -f $(BIN)
