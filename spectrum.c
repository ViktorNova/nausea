/* $Id: spectrum.c,v 1.3 2013/11/18 11:01:51 lostd Exp $ */

/*
 * Add to mpd.conf:
 * audio_output {
 *     type "fifo"
 *     name "Pipe"
 *     path "~/.mpd/mpd.fifo"
 *     format "44100:16:1"
 * }
 */

#include <err.h>
#include <curses.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <fftw3.h>

#define LEN(x) (sizeof (x) / sizeof *(x))

static unsigned msec = 1000 / 25; /* 25 fps */
static unsigned nsamples = 2048; /* mono */
static int samplerate = 44100;
static int bits = 16;
static int channels = 1;
static char symbol = '|';
static int die = 0;
static char *fname = "/tmp/mpd.fifo";
static char *argv0;
static int colors;

struct frame {
	int fd;
	size_t width;
	size_t height;
	size_t nresult;
	int16_t *buf;
	unsigned *res;
	double *in;
	fftw_complex *out;
	fftw_plan plan;
};

/* We assume the screen is 100 pixels in the y direction.
 * To follow the curses convetion (0, 0) is in the top left
 * corner of the screen.  The `min' and `max' values correspond
 * to percentages.  To illustrate this the [0, 40) range gives
 * the top 40% of the screen to the color red.  These values
 * are scaled automatically in the draw() routine to the actual
 * size of the terminal window. */
static struct color_range {
	short pair; /* index in the color table */
	int min;    /* min % */
	int max;    /* max % */
	short fg;   /* foreground color */
	short bg;   /* background color */

	/* these are calculated internally, do not set */
	int scaled_min;
	int scaled_max;
} color_ranges[] = {
	{ 1, 0,  20,  COLOR_RED,    COLOR_BLACK },
	{ 2, 20, 60,  COLOR_YELLOW, COLOR_BLACK },
	{ 3, 60, 100, COLOR_GREEN,  COLOR_BLACK }
};

static void
init(struct frame *fr)
{
	fr->fd = open(fname, O_RDONLY | O_NONBLOCK);
	if (fr->fd == -1)
		err(1, "open");

	fr->nresult = nsamples / 2 + 1;
	fr->buf = malloc(nsamples * sizeof(int16_t));
	fr->res = malloc(fr->nresult * sizeof(unsigned));
	fr->in = fftw_malloc(nsamples * sizeof(double));
	fr->out = fftw_malloc(fr->nresult * sizeof(fftw_complex));
	fr->plan = fftw_plan_dft_r2c_1d(nsamples, fr->in, fr->out, FFTW_ESTIMATE);
}

static void
done(struct frame *fr)
{
	fftw_destroy_plan(fr->plan);
	fftw_free(fr->in);
	fftw_free(fr->out);

	free(fr->buf);
	free(fr->res);

	close(fr->fd);
}

static void
update(struct frame *fr)
{
	ssize_t n, gotsamples;
	unsigned i, j;

	n = read(fr->fd, fr->buf, nsamples * sizeof(int16_t));
	if (n == -1)
		return;

	gotsamples = n / sizeof(int16_t);

	for (i = 0, j = 0; i < nsamples; i++) {
		if (j < gotsamples)
			fr->in[i] = fr->buf[j++];
		else
			fr->in[i] = 0;
	}

	fftw_execute(fr->plan);
}

static void
setcolor(int on, int y)
{
	unsigned i;
	struct color_range *cr;

	if (!colors)
		return;

	for (i = 0; i < LEN(color_ranges); i++) {
		cr = &color_ranges[i];
		if (y >= cr->scaled_min && y < cr->scaled_max) {
			if (on)
				attron(COLOR_PAIR(cr->pair));
			else
				attroff(COLOR_PAIR(cr->pair));
			return;
		}
	}
}

static void
draw(struct frame *fr)
{
	unsigned i, j;
	unsigned freqs_per_col;
	struct color_range *cr;

	if (colors) {
		/* scale color ranges */
		for (i = 0; i < LEN(color_ranges); i++) {
			cr = &color_ranges[i];
			cr->scaled_min = cr->min * LINES / 100;
			cr->scaled_max = cr->max * LINES / 100;
		}
	}

	/* read dimensions to catch window resize */
	fr->width = COLS;
	fr->height = LINES;

	/* take most of the left part of the band */
	freqs_per_col = fr->nresult / fr->width * 6 / 10;
	
	/* scale each frequency to screen */
	for (i = 0; i < fr->nresult; i++)
		fr->res[i] = sqrt(fr->out[i][0] * fr->out[i][0] +
		                  fr->out[i][1] * fr->out[i][1])
		             / 100000 * fr->height / 4;
	
	erase();
	attron(A_BOLD);
	for (i = 0; i < fr->width; i++) {
		size_t bar_height = 0;
		size_t ybegin, yend;

#define MIN(x, y) ((x) < (y) ? (x) : (y))
		for (j = 0; j < freqs_per_col; j++)
			bar_height += fr->res[i * freqs_per_col + j];
		bar_height = MIN(bar_height / freqs_per_col, fr->height);
		ybegin = fr->height - bar_height;
		yend = MIN(bar_height + ybegin, fr->height);
#undef MIN

		/* output symbols */
		for (j = ybegin; j < yend; j++) {
			move(j, i);
			setcolor(1, j);
			printw("%c", symbol);
			setcolor(0, j);
		}
	}
	attroff(A_BOLD);
	refresh();
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-hc] [mpdfifo]\n", argv0);
	fprintf(stderr, "fifo default path is `/tmp/mpd.fifo'\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c;
	unsigned i;
	struct frame fr;
	struct color_range *cr;

	argv0 = argv[0];
	while (--argc > 0 && (*++argv)[0] == '-')
		while ((c = *++argv[0]))
			switch (c) {
			case 'c':
				colors = 1;
				break;
			case 'h':
				/* fall-through */
			default:
				usage();
			}
	if (argc == 1)
		fname = argv[0];
	else if (argc > 1)
		usage();

	/* init fftw3 */
	memset(&fr, 0, sizeof(fr));
	init(&fr);

	/* init curses */
	initscr();
	cbreak();
	noecho();
	nonl();
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	curs_set(FALSE); /* hide cursor */
	timeout(msec);

	if (colors) {
		if (has_colors() == FALSE) {
			endwin();
			done(&fr);
			errx(1, "terminal does not support colors");
		}
		start_color();
		for (i = 0; i < LEN(color_ranges); i++) {
			cr = &color_ranges[i];
			init_pair(cr->pair, cr->fg, cr->bg);
		}
	}

	while (!die) {
		if (getch() == 'q')
			die = 1;

		update(&fr);
		draw(&fr);
	}

	endwin(); /* restore terminal */

	done(&fr); /* destroy context */

	return (0);
}
