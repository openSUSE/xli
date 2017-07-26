/* misc.c:
 *
 * miscellaneous funcs
 *
 * jim frost 10.05.89
 *
 * Copyright 1989, 1990, 1991 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */

#include "copyright.h"
#include "xli.h"
#include "patchlevel"
#include <signal.h>
#include <string.h>

static char *signalName(int sig)
{
	static char buf[32];

	switch (sig) {
	case SIGSEGV:
		return ("SEGV");
	case SIGBUS:
		return ("BUS");
	case SIGFPE:
		return ("FPE");
	case SIGILL:
		return ("ILL");
	default:
		sprintf(buf, "Signal %d", sig);
		return (buf);
	}
}

void memoryExhausted(void)
{
	fprintf(stderr,
		"Memory has been exhausted; Last function was %s, operation cannot continue (sorry).\n", globals.lastfunc);
	if (globals._Xdebug)
		abort();
	else
		exit(1);
}

void internalError(int sig)
{
	static int handling_error = 0;

	switch (handling_error++) {
	case 0:
		printf("\
An internal error (%s) has occurred. Last function was %s.\n\
If you would like to file a bug report, please send email to %s\n\
with a description of how you triggered the bug, the output of xli\n\
before the failure, and the following information:\n\n",
		       signalName(sig), globals.lastfunc, AUTHOR_EMAIL);

		printf("********************************************************************************\n");
		printf("xli Version %s.%s\n", VERSION, PATCHLEVEL);
		tellAboutDisplay(&globals.dinfo);
		printf("********************************************************************************\n\n");
		break;
	case 1:
		fprintf(stderr, "\
An internal error has occurred within the internal error handler.  No more\n\
information about the error is available, sorry.\n");
		break;
	}
	if (globals._Xdebug)	/* dump core if -debug is on */
		abort();
	printf("If you run xli again with the -dumpcore flag, then it will core dump\n");
	printf("so you can investigate the problem yourself.\n");
	exit(1);
}

void version(void)
{
	printf("xli version %s patchlevel %s.\n", VERSION, PATCHLEVEL);
	printf("based on xli version 1 patchlevel 16 by Graeme Gill.\n");
	printf("based on xloadimage version 3 patchlevel 01 by Jim Frost.\n");
	printf("Please send email to %s for bug reports.\n", AUTHOR_EMAIL);
}

void usage(char *name)
{
	version();
	printf("\nUsage: %s [global options] {[image options] image_name ...}\n\n",
	       tail(name));
	printf("\
Type `%s -help [option ...]' for information on a particular option, or\n\
`%s -help' to enter the interactive help facility.\n", tail(name), tail(name));
	exit(1);
}

char *tail(char *path)
{
	int s;
	char *t;

	t = path;
	for (s = 0; *(path + s) != '\0'; s++)
		if (*(path + s) == '/')
			t = path + s + 1;
	return (t);
}

#define MIN(a,b) ( (a)<(b) ? (a) : (b))

Image *processImage(DisplayInfo *dinfo, Image *iimage, ImageOptions *options)
{
	Image *image = iimage, *tmpimage;
	XColor xcolor;

	CURRFUNC("processImage");

	/* Pre-processing */

	/* clip the image if requested */

	if ((options->clipx != 0) || (options->clipy != 0) ||
	    (options->clipw != 0) || (options->cliph != 0)) {
		tmpimage = clip(image, options->clipx, options->clipy,
			(options->clipw ? options->clipw : image->width),
		       (options->cliph ? options->cliph : image->height),
				options);
		if (tmpimage != image && iimage != image)
			freeImage(image);
		image = tmpimage;
	}
	if (options->rotate) {
		tmpimage = rotate(image, options->rotate, globals.verbose);
		if (tmpimage != image && iimage != image)
			freeImage(image);
		image = tmpimage;
	}
	/* zoom image */
	if (options->zoom_auto) {
		if (image->width > globals.dinfo.width * .9)
			options->xzoom = globals.dinfo.width * 90 / image->width;
		else
			options->xzoom = 100;
		if (image->height > globals.dinfo.height * .9)
			options->yzoom = globals.dinfo.height * 90 / image->height;
		else
			options->yzoom = 100;
		/* both dimensions should be shrunk by the same factor */
		options->xzoom = options->yzoom =
			MIN(options->xzoom, options->yzoom);
	}
	if (options->xzoom || options->yzoom) {
		/* if the image is to be blown up, compress before doing it */
		if (!options->colors && RGBP(image) &&	
		    ((!options->xzoom && (options->yzoom > 100)) ||
		     (!options->yzoom && (options->xzoom > 100)) ||
		     (options->xzoom + options->yzoom > 200))) {
			compress_cmap(image, globals.verbose);
		}
		tmpimage = zoom(image, options->xzoom, options->yzoom,
			globals.verbose, TRUE);
		if (tmpimage != image && iimage != image)
			freeImage(image);
		image = tmpimage;
	}

	/* set foreground and background colors of mono image */
	xcolor.flags = DoRed | DoGreen | DoBlue;
	if (image->depth == 1 && (options->fg || options->bg)) {
		int backgmap = 0;
		/* figure out background map number - xwd can make it what it
		 * likes
		 */
		if (*image->rgb.red == 0 && *image->rgb.green == 0 &&
				*image->rgb.blue == 0)
			backgmap = 1;
		if (options->fg) {
			xliParseXColor(dinfo, options->fg, &xcolor);
			xliGammaCorrectXColor(&xcolor, DEFAULT_DISPLAY_GAMMA);
			*(image->rgb.red + (1 - backgmap)) = xcolor.red;
			*(image->rgb.green + (1 - backgmap)) = xcolor.green;
			*(image->rgb.blue + (1 - backgmap)) = xcolor.blue;
		}
		if (options->bg) {
			xliParseXColor(dinfo, options->bg, &xcolor);
			xliGammaCorrectXColor(&xcolor, DEFAULT_DISPLAY_GAMMA);
			*(image->rgb.red + backgmap) = xcolor.red;
			*(image->rgb.green + backgmap) = xcolor.green;
			*(image->rgb.blue + backgmap) = xcolor.blue;
		}
	}

	/* General image processing */
	if (options->smooth > 0) {	/* image is to be smoothed */
		tmpimage = smooth(image, options->smooth, globals.verbose);
		if (tmpimage != image && iimage != image)
			freeImage(image);
		image = tmpimage;
	}

	/* Post-processing */
	if (options->gray)	/* convert image to grayscale */
		gray(image, globals.verbose);

	if (options->normalize) {	/* normalize image */
		tmpimage = normalize(image, globals.verbose);
		if (tmpimage != image && iimage != image)
			freeImage(image);
		image = tmpimage;
	}
	if (options->bright)	/* alter image brightness */
		brighten(image, options->bright, globals.verbose);

	/* forcibly reduce colormap */
	if (options->colors && (TRUEP(image) || (RGBP(image) && (options->colors < image->rgb.used)))) {
		tmpimage = reduce(image, options->colors, options->colordither,
			UNSET_GAMMA, globals.verbose);
		if (tmpimage != image && iimage != image)
			freeImage(image);
		image = tmpimage;
	}

	if (options->dither && (image->depth > 1)) {
		/* image is to be dithered */
		if (options->dither == 1)
			tmpimage = dither(image, globals.verbose);
		else
			tmpimage = halftone(image, globals.verbose);
		if (tmpimage != image && iimage != image)
			freeImage(image);
		image = tmpimage;
		/* Hmmm - if foreground or -background is used, */
		/* make sure it applies here as well */
		if (image->depth == 1 && (options->fg || options->bg)) {
			int backgmap = 0;

			/* figure out background map number -
			 * xwd can make it what it likes
			 */
			if (*image->rgb.red == 0 && *image->rgb.green == 0 &&
					*image->rgb.blue == 0)
				backgmap = 1;
			if (options->fg) {
				xliParseXColor(dinfo, options->fg, &xcolor);
				xliGammaCorrectXColor(&xcolor,
					DEFAULT_DISPLAY_GAMMA);
				image->rgb.red[1 - backgmap] = xcolor.red;
				image->rgb.green[1 - backgmap] = xcolor.green;
				image->rgb.blue[1 - backgmap] = xcolor.blue;
			}

			if (options->bg) {
				xliParseXColor(dinfo, options->bg, &xcolor);
				xliGammaCorrectXColor(&xcolor,
					DEFAULT_DISPLAY_GAMMA);
				image->rgb.red[backgmap] = xcolor.red;
				image->rgb.green[backgmap] = xcolor.green;
				image->rgb.blue[backgmap] = xcolor.blue;
			}
		}
	}

	if (options->expand && !TRUEP(image)) {
		/* expand image to truecolor */
		if (globals.verbose)
			fprintf(stderr, "  Expanding image to TRUE color\n");
		tmpimage = expandtotrue(image);
		if (tmpimage != image && iimage != image)
			freeImage(image);
		image = tmpimage;
	}

	if (RGBP(image) && !image->rgb.compressed) {
		/* make sure colormap is minimized */
		compress_cmap(image, globals.verbose);
	}

	return (image);
}

/* A dumb version that should work reliably
 * search for "s2" in "s1"
 */
char *xlistrstr(char *s1, char *s2)
{
	int n;
	char *p;

	for (n = strlen(s2), p = s1;; p++) {
		if (!(p = index(p, *s2)))
			return (char *) 0;
		if (!strncmp(p, s2, n))
			return p;
	}
}

#ifndef HAS_MEMCPY
/* A block fill routine for BSD style systems */
void bfill(char *s, int n, int c)
{
#if defined(_CRAY)		/* can't do arithmetic on cray pointers */
	while (n >= 8) {
		*s++ = c;
		*s++ = c;
		*s++ = c;
		*s++ = c;
		*s++ = c;
		*s++ = c;
		*s++ = c;
		*s++ = c;
		n -= 8;
	}
	while (n-- > 0)
		*s++ = c;
	return;
#else
	int b;

	/* bytes to next word */
	b = (0 - (int) s) & (sizeof(unsigned long) - 1);
	if (n < b)
		b = n;
	while (n != 0) {
		n -= b;
		while (b-- > 0)
			*s++ = c;
		if (n == 0)
			return;
		/* words to fill */
		b = n & ~(sizeof(unsigned long) - 1);
		if (b != 0) {
			unsigned long f;
			int i;
			f = c & (((unsigned long) -1) >> ((sizeof(unsigned long) - sizeof(char)) * 8));
			for (i = sizeof(char); i < sizeof(unsigned long); i *= 2)
				f |= (f << (8 * i));
			n -= b;	/* remaining count */
			while (b > 0) {
				*((unsigned long *) s) = f;
				s += sizeof(unsigned long);
				b -= sizeof(unsigned long);
			}
		}
		b = n;
	}
#endif
}

#endif

/* An ascii to hex table */

short BEHexTable[256];		/* Big endian conversion value */
static boolean BEHexTableInitialized = FALSE;
short LEHexTable[256];		/* Little endian conversion value */
static boolean LEHexTableInitialized = FALSE;

#define b0000 0			/* things make more sense if you see them by bit */
#define b0001 1
#define b0010 2
#define b0011 3
#define b0100 4
#define b0101 5
#define b0110 6
#define b0111 7
#define b1000 8
#define b1001 9
#define b1010 10
#define b1011 11
#define b1100 12
#define b1101 13
#define b1110 14
#define b1111 15

/* build a hex digit value table.
 */

void initBEHexTable(void)
{
	int a;

	if (BEHexTableInitialized)
		return;

	for (a = 0; a < 256; a++)
		BEHexTable[a] = HEXBAD;

	BEHexTable['0'] = b0000;
	BEHexTable['1'] = b1000;
	BEHexTable['2'] = b0100;
	BEHexTable['3'] = b1100;
	BEHexTable['4'] = b0010;
	BEHexTable['5'] = b1010;
	BEHexTable['6'] = b0110;
	BEHexTable['7'] = b1110;
	BEHexTable['8'] = b0001;
	BEHexTable['9'] = b1001;
	BEHexTable['A'] = b0101;
	BEHexTable['a'] = BEHexTable['A'];
	BEHexTable['B'] = b1101;
	BEHexTable['b'] = BEHexTable['B'];
	BEHexTable['C'] = b0011;
	BEHexTable['c'] = BEHexTable['C'];
	BEHexTable['D'] = b1011;
	BEHexTable['d'] = BEHexTable['D'];
	BEHexTable['E'] = b0111;
	BEHexTable['e'] = BEHexTable['E'];
	BEHexTable['F'] = b1111;
	BEHexTable['f'] = BEHexTable['F'];
	BEHexTable['x'] = HEXSTART_BAD;
	BEHexTable['\r'] = HEXDELIM_IGNORE;
	BEHexTable['\n'] = HEXDELIM_IGNORE;
	BEHexTable['\t'] = HEXDELIM_IGNORE;
	BEHexTable[' '] = HEXDELIM_IGNORE;
	BEHexTable[','] = HEXDELIM_BAD;
	BEHexTable['}'] = HEXDELIM_BAD;


	BEHexTableInitialized = TRUE;
}

void initLEHexTable(void)
{
	int a;

	if (LEHexTableInitialized)
		return;

	for (a = 0; a < 256; a++)
		LEHexTable[a] = HEXBAD;

	LEHexTable['0'] = 0x0;
	LEHexTable['1'] = 0x1;
	LEHexTable['2'] = 0x2;
	LEHexTable['3'] = 0x3;
	LEHexTable['4'] = 0x4;
	LEHexTable['5'] = 0x5;
	LEHexTable['6'] = 0x6;
	LEHexTable['7'] = 0x7;
	LEHexTable['8'] = 0x8;
	LEHexTable['9'] = 0x9;
	LEHexTable['A'] = 0xa;
	LEHexTable['a'] = LEHexTable['A'];
	LEHexTable['B'] = 0xb;
	LEHexTable['b'] = LEHexTable['B'];
	LEHexTable['C'] = 0xc;
	LEHexTable['c'] = LEHexTable['C'];
	LEHexTable['D'] = 0xd;
	LEHexTable['d'] = LEHexTable['D'];
	LEHexTable['E'] = 0xe;
	LEHexTable['e'] = LEHexTable['E'];
	LEHexTable['F'] = 0xf;
	LEHexTable['f'] = LEHexTable['F'];
	LEHexTable['x'] = HEXSTART_BAD;
	LEHexTable['\r'] = HEXDELIM_IGNORE;
	LEHexTable['\n'] = HEXDELIM_IGNORE;
	LEHexTable['\t'] = HEXDELIM_IGNORE;
	LEHexTable[' '] = HEXDELIM_IGNORE;
	LEHexTable[','] = HEXDELIM_BAD;
	LEHexTable['}'] = HEXDELIM_BAD;


	LEHexTableInitialized = TRUE;
}

/* Hext number to integer */
/* return -1 if not legal hex */
int hstoi(unsigned char *s, int n)
{
	int i, c;
	int value = 0;

	if (!LEHexTableInitialized)
		initLEHexTable();
	for (i = 0; i < n; i++) {
		c = s[i];
		c = LEHexTable[c & 0xff];
		if (c < 0)	/* bad hex */
			return (-1);
		value = (value << 4) | c;
	}
	return value;
}
