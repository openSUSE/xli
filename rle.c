/* 
 * rle - read in a Utah RLE Toolkit type image.
 * 
 * Author:      Graeme Gill
 * Date:        30/5/90
 * 
 * Modified 24/5/91 to return 24 bit images if they are found rather
 * than dithering them down to 8 bits.
 *
 */

#define MIN(a,b) ( (a)<(b) ? (a) : (b))

#include "xli.h"
#include "imagetypes.h"
#include "rle.h"

/* input file stuff */
static int ptype;		/* picture type : */
#define BW_NM	0		/* black and white, no map */
#define BW_M	1		/* black and white, and a map */
#define SC_M	2		/* single color channel and color map */
#define C_NM	3		/* full color, no maps */
#define C_M		4	/* full color with color maps */
static rle_pixel **fmaps;	/* file color maps from buildmap() */
static unsigned char **scan;	/* buffer for input data */
static int x_min;		/* copy of picture x_min */
static int y_min;		/* copy of picture y_min */

/* option stuff (Not functional) */
static float img_gam = UNSET_GAMMA;	/* default image gamma (== don't know) */
static int background[3] =
{0, 0, 0};			/* our background color */

int rleIdent(char *fullname, char *name)
{
	ZFILE *rlefile;
	int x_len, y_len;
	int rv;

	rlefile = zopen(fullname);
	if (!rlefile) {
		perror("rleIdent");
		return (0);
	}
	sv_globals.svfb_fd = rlefile;
	rv = rle_get_setup(&sv_globals);
	zclose(rlefile);
	rle_free_setup(&sv_globals);
	switch (rv) {
	case 0:
		/* now figure out the picture type */
		x_len = sv_globals.sv_xmax - sv_globals.sv_xmin + 1;
		y_len = sv_globals.sv_ymax - sv_globals.sv_ymin + 1;
		printf("%s is a %dx%d", name, x_len, y_len);
		switch (sv_globals.sv_ncolors) {
		case 0:
			printf(" RLE image with an no color planes\n");
			return 0;
		case 1:
			switch (sv_globals.sv_ncmap) {
			case 0:
				/* black and white, no map */
				printf(" 8 bit grey scale RLE image with no map\n");
				break;
			case 1:
				/* black and white with a map */
				printf(" 8 bit grey scale RLE image with map\n");
				break;
			case 3:
				/* single channel encoded color with decoding map */
				printf(" 8 bit color RLE image with color map\n");
				break;
			default:
				printf(" 8 bit RLE image with an illegal color map\n");
				return 0;
			}
			break;
		case 3:
			switch (sv_globals.sv_ncmap) {
			case 0:
				printf(" 24 bit color RLE image with no map\n");
				break;
			case 3:
				printf(" 24 bit color RLE image with color map\n");
				break;
			default:
				printf(" 24 bit color RLE image with an illegal color map\n");
				return 0;
			}
			break;
		default:
			printf(" RLE image with an illegal number (%d)of color planes\n", sv_globals.sv_ncolors);
			return 0;
		}
		return 1;
	case -1:		/* Not rle format */
	default:		/* Some sort of error */
		return 0;
	}
}

Image *rleLoad(char *fullname, ImageOptions *image_ops, boolean verbose)
{
	char *name = image_ops->name;
	int x_len, y_len;
	int i, j;
	ZFILE *rlefile;
	int ncol;		/* number of colors */
	int depth;
	unsigned char *bufp;
	Image *image;
	unsigned char *buf;

	CURRFUNC("rleLoad");
	if (!(rlefile = zopen(fullname))) {
		perror("rleLoad");
		return (NULL);
	}
	sv_globals.svfb_fd = rlefile;
	if (rle_get_setup(&sv_globals)) {
		zclose(rlefile);
		rle_free_setup(&sv_globals);
		return (NULL);
	}
	/* Check comments in file for gamma specification */
	{
		char *v;
		if ((v = rle_getcom("image_gamma", &sv_globals)) != NULL) {
			img_gam = atof(v);
			/* Protect against bogus information */
			if (img_gam == UNSET_GAMMA)
				img_gam = 1.0;
			else
				img_gam = 1.0 / img_gam;	/* convert to display gamma */
		} else if ((v = rle_getcom("display_gamma", &sv_globals)) != NULL) {
			img_gam = atof(v);
			/* Protect */
			if (UNSET_GAMMA == img_gam)
				img_gam = 1.0;
		}
	}

	x_len = sv_globals.sv_xmax - sv_globals.sv_xmin + 1;
	y_len = sv_globals.sv_ymax - sv_globals.sv_ymin + 1;

	x_min = sv_globals.sv_xmin;
	y_min = sv_globals.sv_ymin;

	/* fix this so that we don't waste space */
	sv_globals.sv_xmax -= sv_globals.sv_xmin;
	sv_globals.sv_xmin = 0;

	/* turn off the alpha channel (don't waste time and space) */
	sv_globals.sv_alpha = 0;
	SV_CLR_BIT(sv_globals, SV_ALPHA);

	/* for now, force background clear */
	if (sv_globals.sv_background == 1) {	/* danger setting */
		sv_globals.sv_background = 2;
		if (sv_globals.sv_bg_color == 0)	/* if none allocated */
			sv_globals.sv_bg_color = background;	/* use this one */
	}
	/* now figure out the picture type */
	switch (sv_globals.sv_ncolors) {
	case 0:
		fprintf(stderr, "rleLoad: %s - no color channels to display\n", name);
		zclose(rlefile);
		rle_free_setup(&sv_globals);
		return (NULL);
	case 1:
		switch (sv_globals.sv_ncmap) {
		case 0:
			ptype = BW_NM;	/* black and white, no map */
			break;
		case 1:
			ptype = BW_M;	/* black and white with a map */
			break;
		case 3:
			ptype = SC_M;	/* single channel encoded color with decoding map */
			break;
		default:
			fprintf(stderr, "rleLoad: %s - Illegal number of maps for one color channel\n", name);
			zclose(rlefile);
			rle_free_setup(&sv_globals);
			return (NULL);
		}
		break;
	case 3:
		switch (sv_globals.sv_ncmap) {
		case 0:
			ptype = C_NM;	/* color, no map */
			break;
		case 3:
			ptype = C_M;	/* color with maps */
			break;
		default:
			fprintf(stderr, "rleLoad: %s - Illegal number of maps for color picture\n", name);
			zclose(rlefile);
			rle_free_setup(&sv_globals);
			return (NULL);
		}
		break;
	default:
		fprintf(stderr, "rleLoad: %s - too many of color channels (%d)\n", name, sv_globals.sv_ncolors);
		zclose(rlefile);
		rle_free_setup(&sv_globals);
		return (NULL);
	}

	if (verbose) {
		printf("%s is a %dx%d", name, x_len, y_len);
		switch (ptype) {
		case BW_NM:
			printf(" 8 bit grey scale RLE image with no map");
			break;
		case BW_M:
			printf(" 8 bit grey scale RLE image with map");
			break;
		case SC_M:
			printf(" 8 bit RLE image with color map");
			break;
		case C_NM:
			printf(" 24 bit RLE image with no map");
			break;
		case C_M:
			printf(" 24 bit RLE image with color map");
			break;
		}
		if (img_gam != UNSET_GAMMA)
			printf(", with gamma of %4.2f\n", img_gam);
		else
			printf(", with unknown gamma\n");
	}
	znocache(rlefile);

	/* get hold of the color maps */
	fmaps = buildmap(&sv_globals, sv_globals.sv_ncolors, 1.0);

	/* now we had better sort out the picture data */

	/* rle stufff */
	/* Get space for a full color scan line */
	if (ptype != SC_M && ptype != BW_NM) {
		scan = (unsigned char **) lmalloc(sv_globals.sv_ncolors *
						sizeof(unsigned char *));
		for (i = 0; i < sv_globals.sv_ncolors; i++)
			scan[i] = (unsigned char *) lmalloc(x_len);
	}
	if (ptype == C_NM || ptype == C_M) {	/* 24 bit color type result */
		depth = 3;
		image = newTrueImage(x_len, y_len);
		image->title = dupString(name);
	} else {
		depth = 1;
		image = newRGBImage(x_len, y_len, 8 * depth);
		image->title = dupString(name);
	}

	buf = image->data;
	bufp = buf + (y_len - 1) * x_len * depth;
	switch (ptype) {
	case SC_M:
	case BW_NM:
		for (j = y_len; j > 0; j--, bufp -= (x_len * depth)) {
			register unsigned char **bufpp = &bufp;
			if (rle_getrow(&sv_globals, bufpp) < 0)
				break;
		}
		break;
	case BW_M:
		for (j = y_len; j > 0; j--, bufp -= (x_len * depth)) {
			register unsigned char *dp, *r;
			register int i;
			if (rle_getrow(&sv_globals, scan) < 0)
				break;
			for (i = x_len, r = &scan[0][0], dp = bufp; i > 0; i--, r++, dp++)
				*dp = fmaps[0][*r];
		}
		break;
	case C_NM:
		for (j = y_len; j > 0; j--, bufp -= (x_len * depth)) {
			register unsigned char *dp;
			register unsigned char *r, *g, *b;
			register int i;
			if (rle_getrow(&sv_globals, scan) < 0)
				break;
			for (i = x_len, dp = bufp, r = &scan[0][0], g = &scan[1][0], b = &scan[2][0];
			     i > 0; i--, r++, g++, b++) {
				*(dp++) = *r;
				*(dp++) = *g;
				*(dp++) = *b;
			}
		}
		break;
	case C_M:
		for (j = y_len; j > 0; j--, bufp -= (x_len * depth)) {
			register unsigned char *dp;
			register unsigned char *r, *g, *b;
			register int i;
			if (rle_getrow(&sv_globals, scan) < 0)
				break;
			for (i = x_len, dp = bufp, r = &scan[0][0], g = &scan[1][0], b = &scan[2][0];
			     i > 0; i--, r++, g++, b++) {
				*(dp++) = fmaps[0][*r];
				*(dp++) = fmaps[1][*g];
				*(dp++) = fmaps[2][*b];
			}
		}
	}
	if (ptype != SC_M && ptype != BW_NM) {
		/* Free line buffer */
		for (i = 0; i < sv_globals.sv_ncolors; i++)
			lfree(scan[i]);
		lfree((byte *) scan);
	}
	/* Deal with color maps */
	/* now load an appropriate color map */
	if (ptype == SC_M) {
		/* use their maps */
		ncol = 1 << sv_globals.sv_cmaplen;	/* number of entries */
		for (i = 0; i < ncol; i++) {
			*(image->rgb.red + i) = fmaps[0][i] << 8;
			*(image->rgb.green + i) = fmaps[1][i] << 8;
			*(image->rgb.blue + i) = fmaps[2][i] << 8;
		}
		image->rgb.used = ncol;
	} else if (ptype == BW_NM || ptype == BW_M) {
		ncol = 256;	/* don't know whats been used */
		for (i = 0; i < ncol; i++) {
			*(image->rgb.red + i) =
			    *(image->rgb.green + i) =
			    *(image->rgb.blue + i) = i << 8;
		}
		image->rgb.used = ncol;
	}
	/* record the image gamma in the image structure */
	image->gamma = img_gam;

	read_trail_opt(image_ops, rlefile, image, verbose);
	zclose(rlefile);
	freemap(fmaps);
	rle_free_setup(&sv_globals);
	return (image);
}
