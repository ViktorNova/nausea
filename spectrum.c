/* $Id: spectrum.c,v 1.2 2013/11/17 21:55:59 lostd Exp $ */

/*
 * ~/.mpdconf:
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
unsigned samples = 2048; /* mono */
int samplerate = 44100;
int bits = 16;
int channels = 1;
char symbol = '|';
int die = 0;
char *fname = NULL;

struct frame {
	int fd;
	size_t width;
	size_t height;
	size_t nresult;
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

	fr->nresult = samples / 2 + 1;
	fr->res = malloc(fr->nresult * sizeof(unsigned));
	fr->in = fftw_malloc(samples * sizeof(double));
	fr->out = fftw_malloc(fr->nresult * sizeof(fftw_complex));
	fr->plan = fftw_plan_dft_r2c_1d(samples, fr->in, fr->out, FFTW_ESTIMATE);
}

void
spectrum_done(struct frame *fr)
{
	fftw_destroy_plan(fr->plan);
	fftw_free(fr->in);
	fftw_free(fr->out);

	free(fr->res);

	close(fr->fd);
}

void
spectrum_update(struct frame *fr)
{
	int16_t buf[samples];
	ssize_t n, nsamples;
	unsigned i, j;

	n = read(fr->fd, buf, sizeof(buf));
	if (n == -1)
		return;

	nsamples = n / sizeof(int16_t);

	for (i = 0, j = 0; i < samples; i++) {
		if (j < nsamples)
			fr->in[i] = buf[j++];
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
		size_t start_y;
		size_t stop_y;

#define MIN(x, y) ((x) < (y) ? (x) : (y))
		for (j = 0; j < freqs_per_col; j++)
			bar_height += fr->res[i * freqs_per_col + j];
		bar_height = MIN(bar_height / freqs_per_col, fr->height);
		start_y = fr->height - bar_height;
		stop_y = MIN(bar_height + start_y, fr->height);

		/* output symbols */
		for (j = start_y; j < stop_y; j++) {
			move(j, i);
			printw("%c", symbol);
		}
	}
	attroff(A_BOLD);
	refresh();
}

int
main(int argc, char *argv[])
{
	int c, i;
	struct frame fr;

	if (argc != 2)
		errx(1, "usage: %s mpdfifo", argv[0]);
	fname = argv[1];

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
