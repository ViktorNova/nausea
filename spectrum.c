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

unsigned msec = 1000 / 25; /* 25 fps */
unsigned nsamples = 2048; /* mono */
int samplerate = 44100;
int bits = 16;
int channels = 1;
char symbol = '|';
int die = 0;
char *fname = "/tmp/mpd.fifo";
char *argv0;

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

void
spectrum_init(struct frame *fr)
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

void
spectrum_done(struct frame *fr)
{
	fftw_destroy_plan(fr->plan);
	fftw_free(fr->in);
	fftw_free(fr->out);

	free(fr->buf);
	free(fr->res);

	close(fr->fd);
}

void
spectrum_update(struct frame *fr)
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

void
spectrum_draw(struct frame *fr)
{
	unsigned i, j;
	unsigned freqs_per_col;

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
			printw("%c", symbol);
		}
	}
	attroff(A_BOLD);
	refresh();
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-h] [mpdfifo]\n", argv0);
	fprintf(stderr, "fifo default path is `/tmp/mpd.fifo'\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c, i;
	struct frame fr;

	argv0 = argv[0];
	while (--argc > 0 && (*++argv)[0] == '-')
		while ((c = *++argv[0]))
			switch (c) {
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
	spectrum_init(&fr);

	/* init curses */
	initscr();
	cbreak();
	noecho();
	nonl();
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	curs_set(FALSE); /* hide cursor */
	timeout(msec);

	while (!die) {
		if (getch() == 'q')
			die = 1;

		spectrum_update(&fr);
		spectrum_draw(&fr);
	}

	endwin(); /* restore terminal */

	spectrum_done(&fr); /* destroy context */

	return (0);
}
