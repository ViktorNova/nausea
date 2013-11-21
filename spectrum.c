/* $Id: spectrum.c,v 1.5 2013/11/20 15:20:44 lostd Exp $ */

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
static unsigned nsamples = 44100 * 2; /* stereo */
static char symbol = '|';
static char peak = '.';
static char *fname = "/tmp/audio.fifo";
static char *argv0;
static int colors;
static int peaks;
static int die;

/* each bar has a peak that drops */
struct peak {
	int age;
	int pos;
#define PK_HIDDEN -1
};

struct frame {
	int fd;
	size_t width, width_old;
	size_t height;
	struct peak *peaks;
	int16_t *buf;
	unsigned *res;
	double *in;
	fftw_complex *out;
	fftw_plan plan;
};

/* We assume the screen is 100 pixels in the y direction.
 * To follow the curses convetion (0, 0) is in the top left
 * corner of the screen.  The `min' and `max' values correspond
 * to percentages.  To illustrate this the [0, 20) range gives
 * the top 20% of the screen to the color red.  These values
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
	{ 1, 0,  20,  COLOR_RED,    COLOR_BLACK, 0, 0 },
	{ 2, 20, 60,  COLOR_YELLOW, COLOR_BLACK, 0, 0 },
	{ 3, 60, 100, COLOR_GREEN,  COLOR_BLACK, 0, 0 }
};

static void
clearall(struct frame *fr)
{
	unsigned i;

	for (i = 0; i < nsamples / 2; i++) {
		fr->in[i] = 0.;
		fr->out[i][0] = 0.;
		fr->out[i][1] = 0.;
	}
}

static void
init(struct frame *fr)
{
	fr->fd = open(fname, O_RDONLY | O_NONBLOCK);
	if (fr->fd == -1)
		err(1, "open");

	fr->buf = malloc(nsamples * sizeof(int16_t));
	fr->res = malloc(nsamples / 2 * sizeof(unsigned));
	fr->in = fftw_malloc(nsamples / 2 * sizeof(double));
	fr->out = fftw_malloc(nsamples / 2 * sizeof(fftw_complex));

	clearall(fr);

	fr->plan = fftw_plan_dft_r2c_1d(nsamples / 2, fr->in, fr->out,
					FFTW_ESTIMATE);
}

static void
done(struct frame *fr)
{
	fftw_destroy_plan(fr->plan);
	fftw_free(fr->out);
	fftw_free(fr->in);

	free(fr->res);
	free(fr->buf);
	free(fr->peaks);

	close(fr->fd);
}

static void
update(struct frame *fr)
{
	ssize_t n, gotsamples;
	unsigned i;

	n = read(fr->fd, fr->buf, nsamples * sizeof(int16_t));
	if (n == -1) {
		clearall(fr);
		return;
	}

	gotsamples = n / sizeof(int16_t);

	for (i = 0; i < nsamples / 2; i++) {
		fr->in[i] = 0.;
		if (i < gotsamples) {
			/* average the two channels */
			fr->in[i] = fr->buf[i * 2 + 0];
			fr->in[i] += fr->buf[i * 2 + 1];
			fr->in[i] /= 2;
		}
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
	struct peak *pk;

	/* read dimensions to catch window resize */
	fr->width = COLS;
	fr->height = LINES;

	if (peaks) {
		/* change in width needs new peaks */
		if (fr->width != fr->width_old) {
			fr->peaks = realloc(fr->peaks, fr->width * sizeof(struct peak));
			for (i = 0; i < fr->width; i++)
				fr->peaks[i].pos = PK_HIDDEN;
			fr->width_old = fr->width;
		}
	}

	if (colors) {
		/* scale color ranges */
		for (i = 0; i < LEN(color_ranges); i++) {
			cr = &color_ranges[i];
			cr->scaled_min = cr->min * fr->height / 100;
			cr->scaled_max = cr->max * fr->height / 100;
		}
	}

	/* take most of the left part of the band */
#define BANDCUT 0.5
	freqs_per_col = (nsamples / 2) / fr->width * BANDCUT;
#undef BANDCUT

	/* scale each frequency to screen */
#define BARSCALE 0.2
	for (i = 0; i < nsamples / 2; i++) {
		/* cabs() essentially */
		fr->res[i] = sqrt(fr->out[i][0] * fr->out[i][0] +
		                  fr->out[i][1] * fr->out[i][1]);
		/* normalize it */
		fr->res[i] /= (nsamples / 2);
		/* scale it */
		fr->res[i] *= fr->height * BARSCALE;
	}
#undef BARSCALE

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

		pk = &fr->peaks[i];

#define EVERY 1
#define DROP 1
		/* update state for peaks
		 * every n time units drop by m lines */
		if (peaks) {
			if (pk->pos >= ybegin) {
				pk->age = 0;
				pk->pos = ybegin;
			} else {
				pk->age++;
				if ((pk->age % EVERY) == 0)
					pk->pos += DROP;
			}
			/* this freq died out */
			if (fr->height == ybegin && pk->pos == ybegin)
				pk->pos = PK_HIDDEN;
		}
#undef EVERY
#undef DROP

		/* output symbols */
		for (j = ybegin; j < yend; j++) {
			move(j, i);
			setcolor(1, j);
			printw("%c", symbol);
			setcolor(0, j);
		}

		/* output peaks */
		if (peaks && pk->pos != PK_HIDDEN) {
			move(pk->pos, i);
			setcolor(1, pk->pos);
			printw("%c", peak);
			setcolor(0, pk->pos);
		}
	}
	attroff(A_BOLD);
	refresh();
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-hc] [fifo]\n", argv0);
	fprintf(stderr, "default fifo path is `/tmp/audio.fifo'\n");
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
			case 'p':
				peaks = 1;
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
