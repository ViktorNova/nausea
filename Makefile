PREFIX = /usr/local
MANPREFIX = $(PREFIX)/man

CPPFLAGS = -I/usr/local/include
LDFLAGS = -L/usr/local/lib
LDLIBS = -lm -lncursesw -lfftw3

OBJ = nausea.o
BIN = nausea

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

nausea.o: config.h

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)
	install -d $(DESTDIR)$(MANPREFIX)/man1
	install $(BIN).1 $(DESTDIR)$(MANPREFIX)/man1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)
	rm -f $(DESTDIR)$(MANPREFIX)/man1/$(BIN).1

clean:
	rm -f $(BIN) $(OBJ)

.SUFFIXES: .def.h

.def.h.h:
	cp $< $@
