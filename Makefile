PREFIX = /usr/local
MANPREFIX = $(PREFIX)/man

CFLAGS = -I/usr/local/include
LDFLAGS = -L/usr/local/lib
LDLIBS = -lm -lcurses -lfftw3

BIN = spectrum
all: $(BIN)

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)
	install -d $(DESTDIR)$(MANPREFIX)/man1
	install spectrum.1 $(DESTDIR)$(MANPREFIX)/man1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)
	rm -f $(DESTDIR)$(MANPREFIX)/man1/spectrum.1

clean:
	rm -f $(BIN)
