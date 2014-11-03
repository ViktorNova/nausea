PREFIX = /usr/local
MANPREFIX = $(PREFIX)/man

CPPFLAGS = -I/usr/local/include
LDFLAGS = -L/usr/local/lib
LDLIBS = -lm -lcursesw -lfftw3

OBJ = nausea.o
NAME = nausea

all: $(NAME)

nausea.o: config.h

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install $(NAME) $(DESTDIR)$(PREFIX)/bin/$(NAME)
	install -d $(DESTDIR)$(MANPREFIX)/man1
	install $(NAME).1 $(DESTDIR)$(MANPREFIX)/man1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(NAME)
	rm -f $(DESTDIR)$(MANPREFIX)/man1/$(NAME).1

clean:
	rm -f $(NAME) $(OBJ)

.SUFFIXES: .def.h

.def.h.h:
	cp $< $@
