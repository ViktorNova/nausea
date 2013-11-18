PREFIX=/usr/local

CFLAGS = -I/usr/local/include
LDFLAGS = -L/usr/local/lib
LDLIBS = -lm -lcurses -lfftw3

BIN = spectrum
all: $(BIN)

install: all
	@echo installing $(BIN) to $(DESTDIR)$(PREFIX)/bin/$(BIN)
	@cp -f $(BIN) $(DESTDIR)$(PREFIX)/bin

uninstall:
	@echo removing $(BIN) from $(DESTDIR)$(PREFIX)/bin/$(BIN)
	@rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)

clean:
	rm -f $(BIN)
