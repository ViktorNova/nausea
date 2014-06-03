#include <err.h>
#include <complex.h>
#include <curses.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <fftw3.h>

#define LEN(x) (sizeof (x) / sizeof *(x))

static unsigned msec = 1000 / 25; /* 25 fps */
static unsigned nsamples = 44100 * 2; /* stereo */
static char chbar = '|';
static char chpeak = '.';
static char chpoint = '=';
static char *fname = "/tmp/audio.fifo";
static char *argv0;
static int colors;
static int peaks;
static int die;

struct frame {
	int fd;
	size_t width, width_old;
	size_t height;
	int *peak;
#define PK_HIDDEN -1
	int16_t *buf;
	unsigned *res;
	double *in;
	ssize_t gotsamples;
	complex *out;
	fftw_plan plan;
};

/* Supported visualizations:
 * 1 -- spectrum
 * 2 -- wave */
static void draw_spectrum(struct frame *fr);
static void draw_wave(struct frame *fr);
static void (* draw)(struct frame *fr) = draw_spectrum;

/* We assume the screen is 100 pixels in the y direction.
 * To follow the curses convention (0, 0) is in the top left
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

	fr->gotsamples = 0;

	for (i = 0; i < nsamples / 2; i++) {
		fr->in[i] = 0.;
		fr->out[i] = 0. + 0. * I;
	}
}

static void
init(struct frame *fr)
{
	fr->fd = open(fname, O_RDONLY | O_NONBLOCK);
	if (fr->fd == -1)
		err(1, "open %s", fname);

	fr->buf = malloc(nsamples * sizeof(int16_t));
	fr->res = malloc(nsamples / 2 * sizeof(unsigned));
	fr->in = malloc(nsamples / 2 * sizeof(double));
	fr->out = malloc(nsamples / 2 * sizeof(complex));

	clearall(fr);

	fr->plan = fftw_plan_dft_r2c_1d(nsamples / 2, fr->in, fr->out,
					FFTW_ESTIMATE);
}

static void
done(struct frame *fr)
{
	fftw_destroy_plan(fr->plan);
	free(fr->out);
	free(fr->in);

	free(fr->res);
	free(fr->buf);
	free(fr->peak);

	close(fr->fd);
}

static void
update(struct frame *fr)
{
	ssize_t n;
	unsigned i;

	n = read(fr->fd, fr->buf, nsamples * sizeof(int16_t));
	if (n == -1) {
		clearall(fr);
		return;
	}

	fr->gotsamples = n / sizeof(int16_t);

	for (i = 0; i < nsamples / 2; i++) {
		fr->in[i] = 0.;
		if (i < fr->gotsamples / 2) {
			/* average the two channels */
			fr->in[i] = fr->buf[i * 2 + 0];
			fr->in[i] += fr->buf[i * 2 + 1];
			fr->in[i] /= 2.;
		}
	}

	/* compute the DFT if needed */
	if (draw == draw_spectrum)
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
draw_spectrum(struct frame *fr)
{
	unsigned i, j;
	unsigned freqs_per_col;
	struct color_range *cr;

	/* read dimensions to catch window resize */
	fr->width = COLS;
	fr->height = LINES;

	if (peaks) {
		/* change in width needs new peaks */
		if (fr->width != fr->width_old) {
			fr->peak = realloc(fr->peak, fr->width * sizeof(int));
			for (i = 0; i < fr->width; i++)
				fr->peak[i] = PK_HIDDEN;
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
		/* complex absolute value */
		fr->res[i] = cabs(fr->out[i]);
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

		/* compute bar height */
		for (j = 0; j < freqs_per_col; j++)
			bar_height += fr->res[i * freqs_per_col + j];
		bar_height /= freqs_per_col;

		/* we draw from top to bottom */
#define MIN(x, y) ((x) < (y) ? (x) : (y))
		ybegin = fr->height - MIN(bar_height, fr->height);
		yend = fr->height;
#undef MIN

		/* If the current freq reaches the peak, the peak is
		 * updated to that height, else it drops by one line. */
		if (peaks) {
			if (fr->peak[i] >= ybegin)
				fr->peak[i] = ybegin;
			else
				fr->peak[i]++;
			/* this freq died out */
			if (fr->height == ybegin && fr->peak[i] == ybegin)
				fr->peak[i] = PK_HIDDEN;
		}

		/* output bars */
		for (j = ybegin; j < yend; j++) {
			move(j, i);
			setcolor(1, j);
			printw("%c", chbar);
			setcolor(0, j);
		}

		/* output peaks */
		if (peaks && fr->peak[i] != PK_HIDDEN) {
			move(fr->peak[i], i);
			setcolor(1, fr->peak[i]);
			printw("%c", chpeak);
			setcolor(0, fr->peak[i]);
		}
	}
	attroff(A_BOLD);
	refresh();
}

static void
draw_wave(struct frame *fr)
{
	unsigned i, j;
	unsigned samples_per_col;
	double pt_pos, pt_pos_prev = 0, pt_pos_mid;

	/* read dimensions to catch window resize */
	fr->width = COLS;
	fr->height = LINES;

	erase();

	/* not enough samples */
	if (fr->gotsamples < fr->width)
		return;

	samples_per_col = (fr->gotsamples / 2) / fr->width;

	attron(A_BOLD);
	for (i = 0; i < fr->width; i++) {
		size_t y;

		/* compute point position */
		pt_pos = 0;
		for (j = 0; j < samples_per_col; j++)
			pt_pos += fr->in[i * samples_per_col + j];
		pt_pos /= samples_per_col;
		/* normalize it */
		pt_pos /= INT16_MAX;
		/* scale it */
#define PTSCALE 0.8
		pt_pos *= (fr->height / 2) * PTSCALE;
#undef PTSCALE

		/* output points */
		y = fr->height / 2 + pt_pos; /* centering */
		move(y, i);
		printw("%c", chpoint);

		/* Output a helper point by averaging with the previous
		 * position.  This creates a nice effect.  We don't care
		 * about overlaps with the current point. */
		pt_pos_mid = (pt_pos_prev + pt_pos) / 2.0;
		y = fr->height / 2 + pt_pos_mid; /* centering */
		move(y, i);
		printw("%c", chpoint);

		pt_pos_prev = pt_pos;
	}
	attroff(A_BOLD);
	refresh();
}

static int
initcolors(void)
{
	unsigned i;
	struct color_range *cr;

	if (has_colors() == FALSE)
		return -1;
	start_color();
	for (i = 0; i < LEN(color_ranges); i++) {
		cr = &color_ranges[i];
		init_pair(cr->pair, cr->fg, cr->bg);
	}
	return 0;
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-hcp] [fifo]\n", argv0);
	fprintf(stderr, "default fifo path is `/tmp/audio.fifo'\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c;
	struct frame fr;

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

	if (colors && initcolors() < 0) {
		endwin();
		done(&fr);
		errx(1, "your terminal does not support colors");
	}

	while (!die) {
		switch (getch()) {
		case 'q':
			die = 1;
			break;
		case 'c':
			colors = ~colors;
			if (colors)
				(void)initcolors();
			else
				(void)use_default_colors();
			break;
		case 'p':
			peaks = ~peaks;
			break;
		case '1':
			draw = draw_spectrum;
			break;
		case '2':
			draw = draw_wave;
			break;
		}

		update(&fr);
		draw(&fr);
	}

	endwin(); /* restore terminal */

	done(&fr); /* destroy context */

	return (0);
}
