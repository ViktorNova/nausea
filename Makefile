# mpdvis

CFLAGS = -I/usr/local/include
LDFLAGS = -L/usr/local/lib
LDLIBS = -lcurses -lfftw3

BIN = spectrum
all: $(BIN)

clean:
	rm -f $(BIN)
