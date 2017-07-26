/* xpixmap.c:
 *
 * XPixMap format file read and identify routines.  these can handle any
 * "format 1" XPixmap file with up to a memory limited number of chars
 * per pixel. It also handles XPM2 C and XPM3 format files.
 * It is not nearly as picky as it might be.
 *
 * unlike most image loading routines, this is X specific since it
 * requires X color name parsing.  to handle this we have global X
 * variables for display and screen.  it's ugly but it keeps the rest
 * of the image routines clean.
 *
 * Copyright 1989 Jim Frost.  See included file "copyright.h" for complete
 * copyright information.
 *
 * Modified 16/10/92 by GWG to add version 2C and 3 support.
 */

#include "copyright.h"
#include "xli.h"
#include "imagetypes.h"
#include <string.h>

static void freeCtable(char **ctable, int ncolors)
		/* color table */

{
	int i;

	if (ctable == NULL)
		return;
	for (i = 0; i < ncolors; i++)
		if (ctable[i] != NULL)
			lfree(ctable[i]);
	lfree((byte *) ctable);
}

/*
 * put the next word from the string s into b,
 * and return a pointer beyond the word returned.
 * Return NULL if no word is found.
 */
static char *nword(char *s, char *b)
{
	while (*s != '\0' && (*s == ' ' || *s == '\t'))
		s++;
	if (*s == '\0')
		return NULL;
	while (*s != '\0' && *s != ' ' && *s != '\t')
		*b++ = *s++;
	*b = '\0';
	return s;
}

#define XPM_FORMAT1 1
#define XPM_FORMAT2C 2
#define XPM_FORMAT3 3

/* xpm colour map key */
#define XPMKEY_NONE 0
#define XPMKEY_S    1		/* Symbolic (not supported) */
#define XPMKEY_M    2		/* Mono */
#define XPMKEY_G4   3		/* 4-level greyscale */
#define XPMKEY_G    4		/* >4 level greyscale */
#define XPMKEY_C    5		/* color */

char *xpmkeys[6] =
{"", "s", "m", "g4", "g", "c"};

/*
 * encapsulate option processing
 */
int xpmoption(char *s)
{
	int i;
	for (i = XPMKEY_M; i <= XPMKEY_C; i++) {
		if (!strcmp(s, xpmkeys[i]))
			return i;
	}
	printf("Argument to -xpm must be a xpm color context m, g4, g or c. (ignored)\n");
	return XPMKEY_NONE;
}

/*
 * Return TRUE if we think this is an XPM file 
 */

static boolean
isXpixmap(ZFILE * zf, unsigned int *w, unsigned int *h, unsigned int *cpp, unsigned int *ncolors, int *format, char **imagetitle)
			 /* image dimensions */
			 /* chars per pixel */
			 /* number of colors */
			 /* XPM format type */

{
	char buf[BUFSIZ];
	char what[BUFSIZ];
	char *p;
	unsigned int value;
	int maxlines = 8;	/* 8 lines to find #define etc. */
	unsigned int b;
	int c;

	*imagetitle = NULL;

	/* read #defines until we have all that are necessary or until we
	 * get an error
	 */

	*format = *w = *h = *ncolors = 0;
	for (; maxlines-- > 0;) {
		if (!zgets((byte *) buf, BUFSIZ - 1, zf)) {
			if (*imagetitle)
				lfree(*imagetitle);
			return (FALSE);
		}
		if (!strncmp(buf, "#define", 7)) {
			if (sscanf(buf, "#define %s %d", what, &value) != 2) {
				if (*imagetitle)
					lfree(*imagetitle);
				return (FALSE);
			}
			if (!(p = rindex(what, '_')))
				p = what;
			else
				p++;
			if (!strcmp(p, "format"))
				*format = value;
			else if (!strcmp(p, "width"))
				*w = value;
			else if (!strcmp(p, "height"))
				*h = value;
			else if (!strcmp(p, "ncolors"))
				*ncolors = value;

			/* this one is ugly
			 */

			else if (!strcmp(p, "pixel")) {		/* this isn't pretty but it works */
				if (p == what)
					continue;
				*(--p) = '\0';
				if (!(p = rindex(what, '_')) || (p == what) || strcmp(++p, "per"))
					continue;
				*(--p) = '\0';
				if (!(p = rindex(what, '_')))
					p = what;
				if (strcmp(++p, "chars"))
					continue;
				*cpp = value;
			}
		} else if ((sscanf(buf, "static char * %s", what) == 1) &&
			   (p = rindex(what, '_')) && (!strcmp(p + 1, "colors[]") || !strcmp(p + 1, "mono[]"))) {
			zunread(zf, buf, strlen(buf));	/* stuff it back so we can read it again */
			break;
		} else if ((sscanf(buf, "/* %s C */", what) == 1) &&
			   !strcmp(what, "XPM2")) {
			*format = XPM_FORMAT2C;
			break;
		} else if ((sscanf(buf, "/* %s */", what) == 1) &&
			   !strcmp(what, "XPM")) {
			*format = XPM_FORMAT3;
			break;
		}
	}

	if (maxlines <= 0) {
		if (*imagetitle)
			lfree(*imagetitle);
		return FALSE;
	}
	if (*format == XPM_FORMAT2C || *format == XPM_FORMAT3) {
		for (; maxlines-- > 0;) {
			if (!zgets((byte *) buf, BUFSIZ - 1, zf)) {
				if (*imagetitle)
					lfree(*imagetitle);
				return FALSE;
			}
			if (sscanf(buf, "static char * %s", what) == 1) {
				if (strlen(what) >= (unsigned) 2 && what[strlen(what) - 2] == '['
				    && what[strlen(what) - 1] == ']')
					what[strlen(what) - 2] = '\000';	/* remove "[]" */
				if (strlen(what) > (unsigned) 0)	/* if not titleless */
					*imagetitle = dupString(what);
				break;
			}
		}
		if (maxlines <= 0) {
			if (*imagetitle)
				lfree(*imagetitle);
			return FALSE;
		}
		/* get to the first string in the array */
		while (((c = zgetc(zf)) != EOF) && (c != '"'));
		if (c == EOF) {
			if (*imagetitle)
				lfree(*imagetitle);
			return FALSE;
		}
		/* put the string in the buffer */
		for (b = 0; ((c = zgetc(zf)) != EOF) && (c != '"') && b < (BUFSIZ - 1); b++) {
			if (c == '\\')
				c = zgetc(zf);
			if (c == EOF) {
				if (*imagetitle)
					lfree(*imagetitle);
				return FALSE;
			}
			buf[b] = (char) c;
		}
		buf[b] = '\0';
		if (sscanf(buf, "%d %d %d %d", w, h, ncolors, cpp) != 4) {
			if (*imagetitle)
				lfree(*imagetitle);
			return FALSE;
		}
	} else if ((p = rindex(what, '_'))) {
		/* get the name in the image if there is one */
		*p = '\0';
		*imagetitle = dupString(what);
	}
	if (!*format || !*w || !*h || !*ncolors || !*cpp) {
		if (*imagetitle)
			lfree(*imagetitle);
		return FALSE;
	}
	return TRUE;
}

Image *xpixmapLoad(char *fullname, ImageOptions * image_ops, boolean verbose)
{
	ZFILE *zf;
	char *name = image_ops->name;
	char buf[BUFSIZ];
	char what[BUFSIZ];
	char *p;
	char *imagetitle;
	unsigned int value;
	unsigned int w, h;	/* image dimensions */
	unsigned int cpp;	/* chars per pixel */
	unsigned int ncolors;	/* number of colors */
	unsigned int depth;	/* depth of image */
	int format;		/* XPM format type */
	char **ctable = NULL;	/* temp color table */
	int *clookup;		/* working color table */
	int cmin, cmax;		/* min/max color string index numbers */
	Image *image;
	XColor xcolor;
	unsigned int a, b, x, y;
	int c;
	byte *dptr;
	int colkey = image_ops->xpmkeyc;
	int gotkey = XPMKEY_NONE;
	int donecmap = 0;

	if (!(zf = zopen(fullname))) {
		perror("xpixmapLoad");
		return (NULL);
	} {
		unsigned int tw, th;
		unsigned int tcpp;
		unsigned int tncolors;
		if (!isXpixmap(zf, &tw, &th, &tcpp, &tncolors, &format, &imagetitle)) {
			zclose(zf);
			return (NULL);
		}
		w = tw;
		h = th;
		cpp = tcpp;
		ncolors = tncolors;
	}

	for (depth = 1, value = 2; value < ncolors; value <<= 1, depth++);
	image = newRGBImage(w, h, depth);
	image->rgb.used = ncolors;

	if (imagetitle != NULL)
		image->title = imagetitle;
	else
		image->title = dupString(name);

	if (verbose)
		printf("%s is a %dx%d X Pixmap image (Version %d) with %d colors titled '%s'\n",
		       name, w, h, format, ncolors, image->title);

	/*
	 * decide which version of the xpm file to read in
	 */

	if (colkey == XPMKEY_NONE) {	/* if the user hasn't specified it */
		switch (xliDefaultVisual()) {
		case StaticGray:
		case GrayScale:
			switch (xliDefaultDepth()) {
			case 1:
				colkey = XPMKEY_M;
				break;
			case 2:
				colkey = XPMKEY_G4;
				break;
			default:
				colkey = XPMKEY_G;
				break;
			}
			break;
		default:
			colkey = XPMKEY_C;
		}
	}
	for (donecmap = 0; !donecmap;) {	/* until we have read a colormap */
		if (format == XPM_FORMAT1) {
			for (;;) {	/* keep reading lines */
				if (!zgets((byte *) buf, BUFSIZ - 1, zf)) {
					fprintf(stderr, "xpixmapLoad: %s - unable to find a colormap\n", name);
					freeCtable(ctable, ncolors);
					freeImage(image);
					zclose(zf);
					return NULL;
				}
				if ((sscanf(buf, "static char * %s", what) == 1) &&
				    (p = rindex(what, '_')) && (!strcmp(p + 1, "colors[]")
					      || !strcmp(p + 1, "mono[]")
					|| !strcmp(p + 1, "pixels[]"))) {
					if (!strcmp(p + 1, "pixels[]")) {
						if (gotkey == XPMKEY_NONE) {
							fprintf(stderr, "xpixmapLoad: %s - colormap is missing\n", name);
							freeCtable(ctable, ncolors);
							freeImage(image);
							zclose(zf);
							return NULL;
						}
						donecmap = 1;
						break;
					} else if (!strcmp(p + 1, "colors[]")) {
						if (gotkey == XPMKEY_M && colkey == XPMKEY_M) {
							continue;	/* good enough already - look for pixels */
						}
						gotkey = XPMKEY_C;
						break;
					} else {	/* must be m */
						if (gotkey == XPMKEY_C && colkey != XPMKEY_M) {
							continue;	/* good enough already - look for pixels */
						}
						gotkey = XPMKEY_M;
						break;
					}
				}
			}
		}
		if (donecmap)
			break;

		/* read the colors array and build the image colormap
		 */

		znocache(zf);
		ctable = (char **) lmalloc(sizeof(char *) * ncolors);
		bzero(ctable, sizeof(char *) * ncolors);
		xcolor.flags = DoRed | DoGreen | DoBlue;
		for (a = 0; a < ncolors; a++) {

			/* read pixel value
			 */

			*(ctable + a) = (char *) lmalloc(sizeof(char *) * ncolors);
			/* Find the start of the next string */
			while (((c = zgetc(zf)) != EOF) && (c != '"'));
			if (c == EOF) {
				fprintf(stderr, "xpixmapLoad: %s - file ended in the colormap\n", name);
				freeCtable(ctable, ncolors);
				freeImage(image);
				zclose(zf);
				return NULL;
			}
			/* Read cpp characters in as the color symbol */
			for (b = 0; b < cpp; b++) {
				if ((c = zgetc(zf)) == '\\')
					c = zgetc(zf);
				if (c == EOF) {
					fprintf(stderr, "xpixmapLoad: %s - file ended in the colormap\n", name);
					freeCtable(ctable, ncolors);
					freeImage(image);
					zclose(zf);
					return NULL;
				}
				*(*(ctable + a) + b) = (char) c;
			}

			/* Locate the end of this string */
			if (format == XPM_FORMAT1) {
				if (((c = zgetc(zf)) == EOF) || (c != '"')) {
					fprintf(stderr, "xpixmapLoad: %s - file ended in the colormap\n", name);
					freeCtable(ctable, ncolors);
					freeImage(image);
					zclose(zf);
					return NULL;
				}
			}
			/* read color definition and parse it
			 */

			/* Locate the start of the next string */
			if (format == XPM_FORMAT1) {
				while (((c = zgetc(zf)) != EOF) && (c != '"'));
				if (c == EOF) {
					fprintf(stderr, "xpixmapLoad: %s - file ended in the colormap\n", name);
					freeCtable(ctable, ncolors);
					freeImage(image);
					zclose(zf);
					return NULL;
				}
			}
			/* load the rest of this string into the buffer */
			for (b = 0; ((c = zgetc(zf)) != EOF) && (c != '"') && b < (BUFSIZ - 1); b++) {
				if (c == '\\')
					c = zgetc(zf);
				if (c == EOF) {
					fprintf(stderr, "xpixmapLoad: %s - file ended in the colormap\n", name);
					freeCtable(ctable, ncolors);
					freeImage(image);
					zclose(zf);
					return NULL;
				}
				buf[b] = (char) c;
			}
			buf[b] = '\0';

			/* locate the colour to use */
			if (format != XPM_FORMAT1) {
				for (p = buf; p != NULL;) {
					if ((p = nword(p, what)) != NULL
					    && !strcmp(what, xpmkeys[colkey])) {
						if (nword(p, what)) {
							p = what;
							break;	/* found a color definition */
						}
					}
				}
				if (p == NULL) {	/* failed to find that color key type */
					for (b = XPMKEY_C; b >= XPMKEY_M && p == NULL; b--) {	/* try all the rest */
						for (p = buf; p != NULL;) {
							if ((p = nword(p, what)) != NULL
							    && !strcmp(what, xpmkeys[b])) {
								if (nword(p, what)) {
									p = what;
									fprintf(stderr, "xpixmapLoad: %s - couldn't find color key '%s', switching to '%s'\n",
										name, xpmkeys[colkey], xpmkeys[b]);
									colkey = b;
									break;	/* found a color definition */
								}
							}
						}
					}
				}
				if (p == NULL) {
					fprintf(stderr, "xpixmapLoad: %s - file is corrupted\n", name);
					freeCtable(ctable, ncolors);
					freeImage(image);
					zclose(zf);
					return NULL;
				}
				donecmap = 1;	/* There is only 1 color map for new xpm files */
			} else
				p = buf;

			if (!xliParseXColor(&globals.dinfo, p, &xcolor)) {
				fprintf(stderr, "xpixmapLoad: %s - Bad color name '%s'\n", name, p);
				xcolor.red = xcolor.green = xcolor.blue = 0;
			}
			*(image->rgb.red + a) = xcolor.red;
			*(image->rgb.green + a) = xcolor.green;
			*(image->rgb.blue + a) = xcolor.blue;
		}
	}

	/* convert the linear search color table into a lookup table
	 * for better speed (but could use a lot of memory!).
	 */

	cmin = 1 << (sizeof(int) * 8 - 2);	/* Hmm. should use maxint */
	cmax = 0;
	for (a = 0; a < ncolors; a++) {
		int val;
		for (b = 0, val = 0; b < cpp; b++)
			val = val * 96 + ctable[a][b] - 32;
		if (val < cmin)
			cmin = val;
		if (val > cmax)
			cmax = val;
	}
	cmax++;			/* point one past cmax */
	clookup = (int *) lmalloc(sizeof(int) * cmax - cmin);
	bzero(clookup, sizeof(int) * cmax - cmin);
	for (a = 0; a < ncolors; a++) {
		int val;
		for (b = 0, val = 0; b < cpp; b++)
			val = val * 96 + ctable[a][b] - 32;
		clookup[val - cmin] = a;
	}
	freeCtable(ctable, ncolors);

	/*
	 * Now look for the pixel array
	 */

	/* read in image data
	 */

	dptr = image->data;
	for (y = 0; y < h; y++) {
		while (((c = zgetc(zf)) != EOF) && (c != '"'));
		for (x = 0; x < w; x++) {
			int val;
			for (b = 0, val = 0; b < cpp; b++) {
				if ((c = zgetc(zf)) == '\\')
					c = zgetc(zf);
				if (c == EOF) {
					fprintf(stderr, "xpixmapLoad: %s - Short read of X Pixmap\n", name);
					zclose(zf);
					lfree((byte *) clookup);
					return (image);
				}
				val = val * 96 + (int) c - 32;
			}
			if (val < cmin || val >= cmax) {
				fprintf(stderr, "xpixmapLoad: %s - Pixel data doesn't match color data\n", name);
				a = 0;
			}
			a = clookup[val - cmin];
			valToMem((unsigned long) a, dptr, image->pixlen);
			dptr += image->pixlen;
		}
		if ((c = zgetc(zf)) != '"') {
			fprintf(stderr, "xpixmapLoad: %s - Short read of X Pixmap\n", name);
			lfree((byte *) clookup);
			zclose(zf);
			return (image);
		}
	}
	lfree((byte *) clookup);
	read_trail_opt(image_ops, zf, image, verbose);
	zclose(zf);
	return (image);
}

int xpixmapIdent(char *fullname, char *name)
{
	ZFILE *zf;
	char *imagetitle;
	unsigned int w, h;	/* image dimensions */
	unsigned int cpp;	/* chars per pixel */
	unsigned int ncolors;	/* number of colors */
	int format;		/* XPM format type */

	if (!(zf = zopen(fullname))) {
		perror("xpixmapIdent");
		return (0);
	}
	if (!isXpixmap(zf, &w, &h, &cpp, &ncolors, &format, &imagetitle)) {
		zclose(zf);
		return 0;
	}
	if (imagetitle == NULL)
		imagetitle = dupString(name);

	printf("%s is a %dx%d X Pixmap image (Version %d) with %d colors titled '%s'\n",
	       name, w, h, format, ncolors, imagetitle);

	lfree(imagetitle);
	zclose(zf);
	return (1);
}
