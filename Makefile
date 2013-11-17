CFLAGS = -I/usr/local/include
LDFLAGS = -L/usr/local/lib
LDLIBS = -lm -lcurses -lfftw3

BIN = spectrum
all: $(BIN)

clean:
	rm -f $(BIN)
