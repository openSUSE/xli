/* reduce.c
 * 
 * reduce an image's colormap usage to a set number of colors.  this also
 * translates a true color image to a TLA-style image of `n' colors.
 *
 * this function cannot reduce to any number of colors larger than 32768.
 *
 * Based on the file lib/colorquant.c in the Utah Raster Toolkit version 3.0
 *
 * > Perform variance-based color quantization on a "full color" image.
 * > Author:    Craig Kolb
 * >    Department of Mathematics
 * >    Yale University
 * >    kolb@yale.edu
 * > Date:      Tue Aug 22 1989
 * > Copyright (C) 1989 Craig E. Kolb
 *
 * Adaption for xli, faster colour dithering
 * and other improvements by Graeme Gill.
 *
 */

#include "xli.h"

#define MAXCOLORS	32768
#define INPUTQUANT	5

#define REDUCE_GAMMA 2.2	/* Work in gamma compressed space */
				/* since this is closer to a linear */
				/* perceptual space */

/* Ansi uses HUGE_VAL. */
#ifndef HUGE
#define HUGE HUGE_VAL
#endif

typedef struct {
	float weightedvar,	/* weighted variance */
				/* (set to 0 if un-cutable) */
	mean[3];		/* centroid */
	unsigned long weight,	/* # of pixels in box */
	freq[3][256];		/* Projected frequencies */
	int low[3], high[3];	/* Box extent  - within low <= col < high */
} Box;

static void QuantHistogram(Image *image, Box *box);
static void BoxStats(Box *box);
static void UpdateFrequencies(Box *box1, Box *box2);
static void ComputeRGBMap(Box *boxes, int colors, short unsigned int *rgbmap,
	int ditherf);
static void SetRGBmap(int boxnum, Box *box, short unsigned int *rgbmap);
static boolean CutBoxes(Box *boxes, int colors);
static int CutBox(Box *box, Box *newbox);
static int GreatestVariance(Box *boxes, int n);
static boolean FindCutpoint(Box *box, int color, Box *newbox1, Box *newbox2);
static void CopyToNewImage(Image *inimage, Image *outimage,
	unsigned short *rgbmap, int ditherf, int colors, float gamma,
	int verbose);


/* this converts a TLA-style pixel into a 15-bit true color pixel
 */

#define TLA_TO_15BIT(TABLE,PIXEL)           \
  ((((unsigned short)((TABLE).red[PIXEL] & 0xf800)) >> 1) |   \
   (((unsigned short)((TABLE).green[PIXEL] & 0xf800)) >> 6) | \
   (((unsigned short)((TABLE).blue[PIXEL] & 0xf800)) >> 11))

/* this converts a 24-bit true color pixel into a 15-bit true color pixel
 */

#define TRUE_TO_15BIT(PIXEL)     \
  ((((unsigned long)((PIXEL) & 0xf80000)) >> 9) | \
   (((unsigned long)((PIXEL) & 0x00f800)) >> 6) | \
   (((unsigned long)((PIXEL) & 0x0000f8)) >> 3))

/* these macros extract color intensities from a 15-bit true color pixel
 */

#define RED_INTENSITY(P)   (((unsigned short)((P) & 0x7c00)) >> 10)
#define GREEN_INTENSITY(P) (((unsigned short)((P) & 0x03e0)) >> 5)
#define BLUE_INTENSITY(P)   ((unsigned short)((P) & 0x001f))

/*
 * Value corrersponding to full intensity in colormap.  The values placed
 * in the colormap are scaled to be between zero and this number.
 */
#define MAX(x,y)	((x) > (y) ? (x) : (y))

/*
 * Readability constants.
 */
#define REDI		0
#define GREENI		1
#define BLUEI		2

static unsigned long *Histogram,	/* image histogram */
 NPixels;			/* total # of pixels */
#define Bits INPUTQUANT
#define cBits (8-Bits)
#define ColormaxI (1 << Bits)	/* 2 ^ Bits */
#define Colormax (ColormaxI - 1)	/* quantized full-intensity */
#define ColormaxcI (1 << cBits)	/* 2 ^ (8-Bits) */
#define Colormaxc (ColormaxcI - 1)	/* quantized bits lost */

/*
 * if "ditherf" is True, apply color disthering
 * if "Gamma" != 0.0, compensate for gamma post processing
 */
unsigned int verbose;
Image *reduce(Image *image, unsigned colors, int ditherf, float gamma,
	int verbose)
{
	unsigned short *rgbmap;
	Box *Boxes;		/* Array of color boxes. */
	int i;			/* Counter */
	int OutColors;		/* # of entries computed */
	int depth;
	Image *new_image;
	char buf[BUFSIZ];

	CURRFUNC("reduce");

	if (colors > MAXCOLORS)	/* max # of colors we can handle */
		colors = MAXCOLORS;

	if (BITMAPP(image) || (RGBP(image) && image->rgb.used <= colors))
		return (image);

	if (TRUEP(image) && image->pixlen != 3) {
		fprintf(stderr, "reduce: true color image has strange pixel length?\n");
		return (image);
	}
	/* do color reduction and dithering in linear color space */
	if (GAMMA_NOT_EQUAL(image->gamma, REDUCE_GAMMA))
		gammacorrect(image, REDUCE_GAMMA, verbose);

	NPixels = image->width * image->height;

	Histogram = (unsigned long *) lcalloc(ColormaxI * ColormaxI * ColormaxI * sizeof(long));
	Boxes = (Box *) lmalloc(colors * sizeof(Box));
	rgbmap = (unsigned short *) lmalloc(ColormaxI * ColormaxI * ColormaxI * sizeof(unsigned short));

	switch (image->type) {
	case IRGB:
		if (verbose) {
			printf("  Reducing RGB image color usage to %d colors...", colors);
			fflush(stdout);
		}
		QuantHistogram(image, &Boxes[0]);
		break;

	case ITRUE:
		if (verbose) {
			printf("  Converting true color image to RGB image with %d colors...",
			       colors);
			fflush(stdout);
		}
		QuantHistogram(image, &Boxes[0]);
		break;

	default:
		{
			lfree((char *) Histogram);
			lfree((char *) Boxes);
			lfree((char *) rgbmap);
			return (image);		/* not something we can reduce, thank you anyway */
		}
	}

	OutColors = CutBoxes(Boxes, colors);

	/*
	 * We now know the set of representative colors.  We now
	 * must fill in the colormap.
	 */

	/* get destination image */
	depth = colorsToDepth(OutColors);
	new_image = newRGBImage(image->width, image->height, depth);
	snprintf(buf, BUFSIZ, "%s (%d colors)", image->title, OutColors);
	buf[BUFSIZ-1] = '\0';
	new_image->title = dupString(buf);
	new_image->gamma = image->gamma;

	for (i = 0; i < OutColors; i++) {
		new_image->rgb.red[i] =
		    (((unsigned short) (Boxes[i].mean[REDI] + 0.5)) << (cBits + 8));
		new_image->rgb.green[i] =
		    (((unsigned short) (Boxes[i].mean[GREENI] + 0.5)) << (cBits + 8));
		new_image->rgb.blue[i] =
		    (((unsigned short) (Boxes[i].mean[BLUEI] + 0.5)) << (cBits + 8));
	}
	new_image->rgb.used = OutColors;
	new_image->rgb.compressed = TRUE;

	ComputeRGBMap(Boxes, OutColors, rgbmap, ditherf);
	lfree((char *) Histogram);
	lfree((char *) Boxes);

	/* copy old image into new image */

	CopyToNewImage(image, new_image, rgbmap, ditherf, OutColors, gamma, verbose);

	lfree((char *) rgbmap);
	if (verbose)
		printf("done\n");
	return (new_image);
}

/*
 * Compute the histogram of the image as well as the projected frequency
 * arrays for the first world-encompassing box.
 */
static void QuantHistogram(Image *image, Box *box)
{
	register byte *pixel;
	register unsigned long *rf, *gf, *bf;
	int x, y;

	rf = box->freq[0];
	gf = box->freq[1];
	bf = box->freq[2];

	/*
	 * Zero-out the projected frequency arrays of the largest box.
	 * We compute both the histogram and the proj. frequencies of
	 * the first box at the same time to save a pass through the
	 * entire image. 
	 */
	bzero(rf, ColormaxI * sizeof(unsigned long));
	bzero(gf, ColormaxI * sizeof(unsigned long));
	bzero(bf, ColormaxI * sizeof(unsigned long));

	if (image->type == IRGB) {
		Intensity *red = image->rgb.red, *green = image->rgb.green,
		*blue = image->rgb.blue;
		pixel = image->data;
		if (image->pixlen == 1)		/* special case most common this for speed */
			for (y = image->height; y > 0; y--)
				for (x = image->width; x > 0; x--) {
					register unsigned char r, g,
					 b;
					r = red[memToVal(pixel, 1)] >> (8 + cBits);
					g = green[memToVal(pixel, 1)] >> (8 + cBits);
					b = blue[memToVal(pixel, 1)] >> (8 + cBits);
					rf[r]++;
					gf[g]++;
					bf[b]++;
					Histogram[(((r << Bits) | g) << Bits) | b]++;
					pixel += 1;
		} else
			for (y = image->height; y > 0; y--)
				for (x = image->width; x > 0; x--) {
					register unsigned char r, g,
					 b;
					r = red[memToVal(pixel, image->pixlen)] >> (8 + cBits);
					g = green[memToVal(pixel, image->pixlen)] >> (8 + cBits);
					b = blue[memToVal(pixel, image->pixlen)] >> (8 + cBits);
					rf[r]++;
					gf[g]++;
					bf[b]++;
					Histogram[(((r << Bits) | g) << Bits) | b]++;
					pixel += image->pixlen;
				}
	} else {		/* assume ITRUE */
		pixel = image->data;
		if (image->pixlen == 3)		/* most common */
			for (y = image->height; y > 0; y--)
				for (x = image->width; x > 0; x--) {
					register unsigned long a, b;
					while (x > 4) {		/* Unroll 4 times */
						a = (*pixel++) >> cBits;	/* Red */
						rf[a]++;
						b = a << Bits;
						a = (*pixel++) >> cBits;	/* Green */
						gf[a]++;
						b = (b | a) << Bits;
						a = (*pixel++) >> cBits;	/* Blue */
						bf[a]++;
						b |= a;
						Histogram[b]++;
						a = (*pixel++) >> cBits;	/* Red */
						rf[a]++;
						b = a << Bits;
						a = (*pixel++) >> cBits;	/* Green */
						gf[a]++;
						b = (b | a) << Bits;
						a = (*pixel++) >> cBits;	/* Blue */
						bf[a]++;
						b |= a;
						Histogram[b]++;
						a = (*pixel++) >> cBits;	/* Red */
						rf[a]++;
						b = a << Bits;
						a = (*pixel++) >> cBits;	/* Green */
						gf[a]++;
						b = (b | a) << Bits;
						a = (*pixel++) >> cBits;	/* Blue */
						bf[a]++;
						b |= a;
						Histogram[b]++;
						a = (*pixel++) >> cBits;	/* Red */
						rf[a]++;
						b = a << Bits;
						a = (*pixel++) >> cBits;	/* Green */
						gf[a]++;
						b = (b | a) << Bits;
						a = (*pixel++) >> cBits;	/* Blue */
						bf[a]++;
						b |= a;
						Histogram[b]++;
						x -= 4;
					}
					a = (*pixel++) >> cBits;	/* Red */
					rf[a]++;
					b = a << Bits;
					a = (*pixel++) >> cBits;	/* Green */
					gf[a]++;
					b = (b | a) << Bits;
					a = (*pixel++) >> cBits;	/* Blue */
					bf[a]++;
					b |= a;
					Histogram[b]++;
		} else		/* less common */
			for (y = image->height; y > 0; y--)
				for (x = image->width; x > 0; x--) {
					register unsigned char r, g,
					 b;
					r = TRUE_RED(memToVal(pixel, image->pixlen)) >> cBits;
					rf[r]++;
					g = TRUE_GREEN(memToVal(pixel, image->pixlen)) >> cBits;
					gf[g]++;
					b = TRUE_BLUE(memToVal(pixel, image->pixlen)) >> cBits;
					bf[b]++;
					Histogram[(((r << Bits) | g) << Bits) | b]++;
					pixel += image->pixlen;
				}
	}
}

/*
   * Interatively cut the boxes.
 */
static int CutBoxes(Box *boxes, int colors)
{
	int curbox;

	boxes[0].low[REDI] = boxes[0].low[GREENI] = boxes[0].low[BLUEI] = 0;
	boxes[0].high[REDI] = boxes[0].high[GREENI] =
	    boxes[0].high[BLUEI] = ColormaxI;
	boxes[0].weight = NPixels;

	BoxStats(&boxes[0]);

	for (curbox = 1; curbox < colors;) {
		int n;
		n = GreatestVariance(boxes, curbox);
		if (n == curbox)
			break;	/* all are un-cutable */
		if (CutBox(&boxes[n], &boxes[curbox]))
			curbox++;	/* cut successfully */
	}

	return curbox;
}

/*
 * Return the number of the box in 'boxes' with the greatest variance.
 * Restrict the search to those boxes with indices between 0 and n-1.
 * Return n if none of the boxes are cutable.
 */
static int GreatestVariance(Box *boxes, int n)
{
	register int i, whichbox = n;
	float max = 0.0;

	for (i = 0; i < n; i++) {
		if (boxes[i].weightedvar > max) {
			max = boxes[i].weightedvar;
			whichbox = i;
		}
	}
	return whichbox;
}

/* Compute mean and weighted variance of the given box. */
static void BoxStats(Box *box)
{
	register int i, color;
	unsigned long *freq;
	float mean, var;

	if (box->weight == 0) {
		box->weightedvar = 0.0;
		return;
	}
	box->weightedvar = 0.;
	for (color = 0; color < 3; color++) {
		var = mean = 0;
		i = box->low[color];
		freq = &box->freq[color][i];
		for (; i < box->high[color]; i++, freq++) {
			mean += i * *freq;
			var += i * i * *freq;
		}
		box->mean[color] = mean / (float) box->weight;
		box->weightedvar += var - box->mean[color] * box->mean[color] *
		    (float) box->weight;
	}
	box->weightedvar /= NPixels;
}

/*
 * Cut the given box.  Returns TRUE if the box could be cut,
 * FALSE (and weightedvar == 0.0) otherwise.
 */
static boolean CutBox(Box *box, Box *newbox)
{
	int i;
	double totalvar[3];
	Box newboxes[3][2];

	if (box->weightedvar == 0.0 || box->weight == 0) {
		/*
		 * Can't cut this box.
		 */
		box->weightedvar = 0.0;		/* Don't try cutting it again */
		return FALSE;
	}
	/*
	 * Find 'optimal' cutpoint along each of the red, green and blue
	 * axes.  Sum the variances of the two boxes which would result
	 * by making each cut and store the resultant boxes for 
	 * (possible) later use.
	 */
	for (i = 0; i < 3; i++) {
		if (FindCutpoint(box, i, &newboxes[i][0], &newboxes[i][1]))
			totalvar[i] = newboxes[i][0].weightedvar +
			    newboxes[i][1].weightedvar;
		else
			totalvar[i] = HUGE;	/* Not cutable on that axis */
	}

	/*
	 * Find which of the three cuts minimized the total variance
	 * and make that the 'real' cut.
	 */
	if (totalvar[REDI] < HUGE &&
	    totalvar[REDI] <= totalvar[GREENI] &&
	    totalvar[REDI] <= totalvar[BLUEI]) {
		*box = newboxes[REDI][0];
		*newbox = newboxes[REDI][1];
		return TRUE;
	} else if (totalvar[GREENI] < HUGE &&
		   totalvar[GREENI] <= totalvar[REDI] &&
		   totalvar[GREENI] <= totalvar[BLUEI]) {
		*box = newboxes[GREENI][0];
		*newbox = newboxes[GREENI][1];
		return TRUE;
	} else if (totalvar[BLUEI] < HUGE) {
		*box = newboxes[BLUEI][0];
		*newbox = newboxes[BLUEI][1];
		return TRUE;
	}
	/* Unable to cut box on any axis - don't try again */
	box->weightedvar = 0.0;
	return FALSE;
}

/*
 * Compute the 'optimal' cutpoint for the given box along the axis
 * indcated by 'color'.  Store the boxes which result from the cut
 * in newbox1 and newbox2.
 * If it is not possible to 
 */
static boolean FindCutpoint(Box *box, int color, Box *newbox1, Box *newbox2)
{
	float u, v, max;
	int i, maxindex, minindex, cutpoint;
	unsigned long optweight, curweight;

	if (box->low[color] + 1 == box->high[color])
		return FALSE;	/* Cannot be cut. */
	minindex = (int) (((float) box->low[color] + box->mean[color]) * 0.5);
	maxindex = (int) ((box->mean[color] + (float) box->high[color]) * 0.5);

	cutpoint = minindex;
	optweight = box->weight;

	curweight = 0;
	for (i = box->low[color]; i < minindex; i++)
		curweight += box->freq[color][i];
	u = 0.0;
	max = -1.0;
	for (i = minindex; i <= maxindex; i++) {
		curweight += box->freq[color][i];
		if (curweight == box->weight)
			break;
		u += (float) (i * box->freq[color][i]) /
		    (float) box->weight;
		v = ((float) curweight / (float) (box->weight - curweight)) *
		    (box->mean[color] - u) * (box->mean[color] - u);
		if (v > max) {
			max = v;
			cutpoint = i;
			optweight = curweight;
		}
	}
	cutpoint++;
	*newbox1 = *newbox2 = *box;
	newbox1->weight = optweight;
	newbox2->weight -= optweight;
	if (newbox1->weight == 0 || newbox2->weight == 0)
		return FALSE;	/* Unable to cut on this axis */
	newbox1->high[color] = cutpoint;
	newbox2->low[color] = cutpoint;
	UpdateFrequencies(newbox1, newbox2);
	BoxStats(newbox1);
	BoxStats(newbox2);

	return TRUE;		/* Found cutpoint. */
}

/*
 * Update projected frequency arrays for two boxes which used to be
 * a single box. Also shrink the box sizes to fit the points.
 */
static void UpdateFrequencies(Box *box1, Box *box2)
{
	register unsigned long myfreq, *h;
	register int b, g, r;
	int roff;
	register unsigned long *b1f0, *b1f1, *b1f2, *b2f0, *b2f1, *b2f2;

	b1f0 = &box1->freq[0][0];
	b1f1 = &box1->freq[1][0];
	b1f2 = &box1->freq[2][0];
	b2f0 = &box2->freq[0][0];
	b2f1 = &box2->freq[1][0];
	b2f2 = &box2->freq[2][0];

	bzero(b1f0, ColormaxI * sizeof(unsigned long));
	bzero(b1f1, ColormaxI * sizeof(unsigned long));
	bzero(b1f2, ColormaxI * sizeof(unsigned long));

	for (r = box1->low[0]; r < box1->high[0]; r++) {
		roff = r << Bits;
		for (g = box1->low[1]; g < box1->high[1]; g++) {
			b = box1->low[2];
			h = Histogram + (((roff | g) << Bits) | b);
			for (; b < box1->high[2]; b++) {
				myfreq = *h++;
				if (myfreq == 0)
					continue;
				b1f0[r] += myfreq;
				b1f1[g] += myfreq;
				b1f2[b] += myfreq;
				b2f0[r] -= myfreq;
				b2f1[g] -= myfreq;
				b2f2[b] -= myfreq;
			}
		}
	}

	/* shrink the boxes to fit the new points */
	for (r = 0; r < 3; r++) {
		register int l1, l2, h1, h2;
		l1 = l2 = ColormaxI;
		h1 = h2 = 0;

		for (g = 0; g < ColormaxI; g++) {
			if (box1->freq[r][g] != 0) {
				if (g < l1)
					l1 = g;
				if (g > h1)
					h1 = g;
			}
			if (box2->freq[r][g] != 0) {
				if (g < l2)
					l2 = g;
				if (g > h2)
					h2 = g;
			}
		}
		box1->low[r] = l1;
		box2->low[r] = l2;
		box1->high[r] = h1 + 1;
		box2->high[r] = h2 + 1;
	}
}

/* Compute RGB to colormap index map. */
static void ComputeRGBMap(Box *boxes, int colors, unsigned short *rgbmap,
	int ditherf)
{
	register int i;

	if (!ditherf) {
		/*
		 * The centroid of each box serves as the representative
		 * for each color in the box.
		 */
		for (i = 0; i < colors; i++)
			SetRGBmap(i, &boxes[i], rgbmap);
	} else {
		/* Initialise the pixel value to illegal value.
		 * Will fill in entry on demand.
		 */
		bfill((char *) rgbmap, ColormaxI * ColormaxI * ColormaxI
			* sizeof(*rgbmap), 0xff);
		/* speed things up by pre-filling boxes with input colors */
		for (i = 0; i < colors; i++)
			SetRGBmap(i, &boxes[i], rgbmap);
	}
}

/*
 * Make the centroid of "boxnum" serve as the representative for
 * each color in the box.
 */
static void SetRGBmap(int boxnum, Box *box, unsigned short *rgbmap)
{
	register int r, g, b;

	for (r = box->low[REDI]; r < box->high[REDI]; r++) {
		for (g = box->low[GREENI]; g < box->high[GREENI]; g++) {
			for (b = box->low[BLUEI]; b < box->high[BLUEI]; b++) {
				rgbmap[(((r << Bits) | g) << Bits) | b] = (unsigned short) boxnum;
			}
		}
	}
}


/* nearest neighbor structure */
typedef struct {
	int length;		/* number of colors */
	unsigned char *red, *green, *blue;
	unsigned short *pixel;
} NN;

#define NNBits 3
#define NNcBits (8-NNBits)
#define NNmaxI (1 << NNBits)	/* 2 ^ Bits */
#define NNmax (NNmaxI - 1)	/* quantized full-intensity */
#define NNmaxcI (1 << NNcBits)	/* 2 ^ (8 - Bits) */
#define NNmaxc (NNmaxcI - 1)	/* quantized bits lost */

/* Initialise the appropriate nn cell */
static void find_nnearest(int rval, int gval, int bval, NN *nnp,
	Image *outimage)
{
	long bdist, mdist;
	Intensity *red = outimage->rgb.red, *green = outimage->rgb.green,
	*blue = outimage->rgb.blue;
	long *ds, *de, dists[MAXCOLORS];
	int *is, indexes[MAXCOLORS];
	int i, length = 0;
	double tdouble;

	rval = ((rval & ~NNmaxc) << 1) + NNmaxc;	/* closest color to center of cell */
	gval = ((gval & ~NNmaxc) << 1) + NNmaxc;	/* (2 * scaled values) */
	bval = ((bval & ~NNmaxc) << 1) + NNmaxc;

#define SQUARE(aa) ((aa) * ( aa))
#define QERR(r1,g1,b1,r2,g2,b2) /* sum of squares distance (in 2 * scale) */ \
	(SQUARE(r1 - (r2 >>7)) +  \
	 SQUARE(g1 - (g2 >>7)) +  \
	 SQUARE(b1 - (b2 >>7)))

	/* Calculate the distances of all the colors in the colormap */
	/* from the center of this nn cell. */
	/* (unroll this loop 4 times) */
	for (ds = dists, de = ds + outimage->rgb.used; ds < (de - 3);) {
		*ds = QERR(rval, gval, bval, *red, *green, *blue);
		ds++;
		red++;
		green++;
		blue++;
		*ds = QERR(rval, gval, bval, *red, *green, *blue);
		ds++;
		red++;
		green++;
		blue++;
		*ds = QERR(rval, gval, bval, *red, *green, *blue);
		ds++;
		red++;
		green++;
		blue++;
		*ds = QERR(rval, gval, bval, *red, *green, *blue);
		ds++;
		red++;
		green++;
		blue++;
	}
	/* finish off remaining */
	while (ds < de) {
		*ds = QERR(rval, gval, bval, *red, *green, *blue);
		ds++;
		red++;
		green++;
		blue++;
	}

	/* find nearest colormap colour to the center */
	/* (unroll this loop 8 times) */
	for (bdist = 0x7fffffff, ds = dists; ds < (de - 7); ds++) {
		if (*ds < bdist)
			goto found_small;
		ds++;
		if (*ds < bdist)
			goto found_small;
		ds++;
		if (*ds < bdist)
			goto found_small;
		ds++;
		if (*ds < bdist)
			goto found_small;
		ds++;
		if (*ds < bdist)
			goto found_small;
		ds++;
		if (*ds < bdist)
			goto found_small;
		ds++;
		if (*ds < bdist)
			goto found_small;
		ds++;
		if (*ds >= bdist)
			continue;
	      found_small:
		bdist = *ds;
	}
	/* finish off remaining */
	while (ds < de) {
		if (*ds < bdist)
			bdist = *ds;
		ds++;
	}

	/* figure out the maximum distance from center */
	tdouble = sqrt((double) bdist) + sqrt((double) (NNmaxc * NNmaxc * 12));
	mdist = (int) ((tdouble * tdouble) + 0.5);	/* back to square distance */

	/* now select the neighborhood groups */
	/* (unroll this loop 8 times) */
	for (is = indexes, ds = dists; ds < (de - 7); ds++) {
		if (*ds <= mdist)
			goto found_neighbor;
		ds++;
		if (*ds <= mdist)
			goto found_neighbor;
		ds++;
		if (*ds <= mdist)
			goto found_neighbor;
		ds++;
		if (*ds <= mdist)
			goto found_neighbor;
		ds++;
		if (*ds <= mdist)
			goto found_neighbor;
		ds++;
		if (*ds <= mdist)
			goto found_neighbor;
		ds++;
		if (*ds <= mdist)
			goto found_neighbor;
		ds++;
		if (*ds > mdist)
			continue;
	      found_neighbor:
		*is++ = ds - dists;
	}
	/* finish off remaining */
	for (; ds < de; ds++) {
		if (*ds <= mdist)
			*is++ = ds - dists;
	}

	/* now make up the entries in the neighborhood structure entry */
	length = is - indexes;
	nnp->length = length;
	nnp->red = (unsigned char *) lmalloc(length * 3);
	nnp->green = nnp->red + length;
	nnp->blue = nnp->green + length;
	nnp->pixel = (unsigned short *) lmalloc(length * sizeof(unsigned short));
	red = outimage->rgb.red;
	green = outimage->rgb.green;
	blue = outimage->rgb.blue;
	for (i = 0, is = indexes; i < length; i++, is++) {
		nnp->red[i] = red[*is] >> 8;
		nnp->green[i] = green[*is] >> 8;
		nnp->blue[i] = blue[*is] >> 8;
		nnp->pixel[i] = *is;
	}

#undef SQUARE
#undef QERR
}

/*
 * Find the neareset chosen pixel value to the given color.
 * Uses nn accelerator.
 */
static unsigned short find_nearest(int rval, int gval, int bval, NN *nna,
	Image *outimage)
{
	long bdist, ndist;
	int i, bindex = 0;
	int lastc;
	unsigned char *red, *green, *blue;
	int nnindex;
	NN *nnp;

	nnindex = ((((rval >> NNcBits) << NNBits) | (gval >> NNcBits)) << NNBits) | (bval >> NNcBits);

	nnp = nna + nnindex;
	if (nnp->length == 0)	/* Create nearest neighbor color mapping on demand */
		find_nnearest(rval, gval, bval, nnp, outimage);
	lastc = nnp->length - 1;
	red = &nnp->red[lastc],
	    green = &nnp->green[lastc],
	    blue = &nnp->blue[lastc];

	rval = (rval & ~Colormaxc) + (ColormaxcI >> 1);		/* closest color to center of 15 bit color */
	gval = (gval & ~Colormaxc) + (ColormaxcI >> 1);
	bval = (bval & ~Colormaxc) + (ColormaxcI >> 1);

#define SQUARE(aa) ((aa) * ( aa))
#define QERR(r1,g1,b1,r2,g2,b2) \
	(SQUARE(r1 - r2) +  \
	 SQUARE(g1 - g2) +  \
	 SQUARE(b1 - b2))

	/* now find the the nearest colour */
	/* (try and unroll this loop 8 times) */
	for (bdist = 0x7fffffff, i = lastc; i >= 7; i--, red--, green--, blue--) {
		if ((ndist = QERR(rval, gval, bval, *red, *green, *blue)) < bdist)
			goto found_small;
		i--;
		red--;
		green--;
		blue--;
		if ((ndist = QERR(rval, gval, bval, *red, *green, *blue)) < bdist)
			goto found_small;
		i--;
		red--;
		green--;
		blue--;
		if ((ndist = QERR(rval, gval, bval, *red, *green, *blue)) < bdist)
			goto found_small;
		i--;
		red--;
		green--;
		blue--;
		if ((ndist = QERR(rval, gval, bval, *red, *green, *blue)) < bdist)
			goto found_small;
		i--;
		red--;
		green--;
		blue--;
		if ((ndist = QERR(rval, gval, bval, *red, *green, *blue)) < bdist)
			goto found_small;
		i--;
		red--;
		green--;
		blue--;
		if ((ndist = QERR(rval, gval, bval, *red, *green, *blue)) < bdist)
			goto found_small;
		i--;
		red--;
		green--;
		blue--;
		if ((ndist = QERR(rval, gval, bval, *red, *green, *blue)) < bdist)
			goto found_small;
		i--;
		red--;
		green--;
		blue--;
		if ((ndist = QERR(rval, gval, bval, *red, *green, *blue)) >= bdist)
			continue;
	      found_small:
		bdist = ndist;
		bindex = i;
	}
	/* finish off remaining */
	for (; i >= 0; i--, red--, green--, blue--) {
		if ((ndist = QERR(rval, gval, bval, *red, *green, *blue)) < bdist) {
			bdist = ndist;
			bindex = i;
		}
	}
	return nnp->pixel[bindex];
#undef SQUARE
#undef QERR
}

/* "rgbmap" is pixel value lookup map */
static void CopyToNewImage(Image *inimage, Image *outimage,
	unsigned short *rgbmap, int ditherf, int colors, float gamma,
	int verbose)
{
	byte *pixel, *dpixel;
	int x, y;

	pixel = inimage->data;
	dpixel = outimage->data;

	if (!ditherf) {
		switch (inimage->type) {
		case IRGB:
			if (outimage->pixlen == 1 && inimage->pixlen == 1)	/* most common */
				for (y = inimage->height; y > 0; y--)
					for (x = inimage->width; x > 0; x--) {
						register unsigned long temp;
						temp = TLA_TO_15BIT(inimage->rgb, *pixel);
						temp = rgbmap[temp];
						valToMem(temp, dpixel, 1);
						pixel++;
						dpixel++;
			} else	/* less common */
				for (y = inimage->height; y > 0; y--)
					for (x = inimage->width; x > 0; x--) {
						register unsigned long temp;
						temp = memToVal(pixel, inimage->pixlen);
						temp = TLA_TO_15BIT(inimage->rgb, temp);
						temp = rgbmap[temp];
						valToMem(temp, dpixel, outimage->pixlen);
						pixel += inimage->pixlen;
						dpixel += outimage->pixlen;
					}
			break;

		case ITRUE:
			if (inimage->pixlen == 3 && outimage->pixlen == 1) {	/* common */
				for (y = inimage->height; y > 0; y--) {
					register unsigned long temp;
					x = inimage->width;
					while (x >= 4) {	/* Unroll 4 times */
						temp = (((unsigned long) *pixel) & 0xf8) << (Bits + Bits - cBits);
						pixel++;
						temp |= (((unsigned long) *pixel) & 0xf8) << (Bits - cBits);
						pixel++;
						temp |= ((unsigned long) *pixel) >> cBits;
						*dpixel = rgbmap[temp];
						pixel++;
						dpixel++;
						temp = (((unsigned long) *pixel) & 0xf8) << (Bits + Bits - cBits);
						pixel++;
						temp |= (((unsigned long) *pixel) & 0xf8) << (Bits - cBits);
						pixel++;
						temp |= ((unsigned long) *pixel) >> cBits;
						*dpixel = rgbmap[temp];
						pixel++;
						dpixel++;
						temp = (((unsigned long) *pixel) & 0xf8) << (Bits + Bits - cBits);
						pixel++;
						temp |= (((unsigned long) *pixel) & 0xf8) << (Bits - cBits);
						pixel++;
						temp |= ((unsigned long) *pixel) >> cBits;
						*dpixel = rgbmap[temp];
						pixel++;
						dpixel++;
						temp = (((unsigned long) *pixel) & 0xf8) << (Bits + Bits - cBits);
						pixel++;
						temp |= (((unsigned long) *pixel) & 0xf8) << (Bits - cBits);
						pixel++;
						temp |= ((unsigned long) *pixel) >> cBits;
						*dpixel = rgbmap[temp];
						pixel++;
						dpixel++;
						x -= 4;
					}
					while (x >= 1) {	/* Finish remaining */
						temp = (((unsigned long) *pixel) & 0xf8) << (Bits + Bits - cBits);
						pixel++;
						temp |= (((unsigned long) *pixel) & 0xf8) << (Bits - cBits);
						pixel++;
						temp |= ((unsigned long) *pixel) >> cBits;
						*dpixel = rgbmap[temp];
						pixel++;
						dpixel++;
						x--;
					}
				}
			} else	/* less common */
				for (y = inimage->height; y > 0; y--)
					for (x = inimage->width; x > 0; x--) {
						register unsigned long temp;
						temp = memToVal(pixel, inimage->pixlen);
						temp = TRUE_TO_15BIT(temp);
						temp = rgbmap[temp];
						valToMem(temp, dpixel, outimage->pixlen);
						pixel += inimage->pixlen;
						dpixel += outimage->pixlen;
					}
			break;
		}
	} else {		/* else use dithering */
		NN *nna, *nnp;
		Intensity *ored, *ogreen, *oblue;
		int satary[3 * 256 + 20], *sat = &satary[256 + 10];
		int *epixel, *ip, nextr, nextg, nextb;
		int err1ary[512], *err1 = &err1ary[256];
		int err3ary[512], *err3 = &err3ary[256];
		int err5ary[512], *err5 = &err5ary[256];
		int err7ary[512], *err7 = &err7ary[256];

		if (verbose) {
			printf("\n  (Using color dithering to reduce the impact of restricted colors)...");
		}
		nna = (NN *) lcalloc((NNmaxI * NNmaxI * NNmaxI) * sizeof(NN));

		/* Init saturation table */
		for (x = -256 - 10; x < (512 + 10); x++) {
			y = x;
			if (y < 0)
				y = 0;
			else if (y > 255)
				y = 255;
			sat[x] = y;
		}
		{
			int maxerr;	/* Fudge factor */
			maxerr = (int) (256.0 / pow(((double) colors), 0.45));
			/* Init error lookup array */
			for (x = -255; x < 256; x++) {
				int err, ro;
				err = x;
				if (err > maxerr)
					err = maxerr;
				else if (err < -maxerr)
					err = -maxerr;
				err7[x] = (err * 7) / 16;
				err5[x] = (err * 5) / 16;
				err3[x] = (err * 3) / 16;
				err1[x] = (err * 1) / 16;
				ro = err - err1[x] - err3[x] - err5[x] - err7[x];
				switch (err & 3) {	/* Distribute round off error */
				case 0:
					err1[x] += ro;
					break;
				case 1:
					err5[x] += ro;
					break;
				case 2:
					err3[x] += ro;
					break;
				case 3:
					err7[x] += ro;
					break;
				}
			}
		}
		epixel = ((int *) lcalloc((inimage->width + 2) * 3 * sizeof(int))) + 3;

		/* set up index mapping tables */
		ored = (Intensity *) lmalloc(sizeof(Intensity) * outimage->rgb.size);
		ogreen = (Intensity *) lmalloc(sizeof(Intensity) * outimage->rgb.size);
		oblue = (Intensity *) lmalloc(sizeof(Intensity) * outimage->rgb.size);
		if (UNSET_GAMMA == gamma || (gamma / inimage->gamma) == 1.0)
			for (x = 0; x < outimage->rgb.size; x++) {
				ored[x] = outimage->rgb.red[x] >> 8;
				ogreen[x] = outimage->rgb.green[x] >> 8;
				oblue[x] = outimage->rgb.blue[x] >> 8;
		} else
			/* Create map that accounts for quantizing effect of output */
			/* gamma mapping applied to small values. */
			for (x = 0; x < outimage->rgb.size; x++) {
				int val;
				val = outimage->rgb.red[x] >> 8;
				val = (int) (0.5 + 255 * pow(val / 255.0, inimage->gamma / gamma));
				val = (int) (0.5 + 255 * pow(val / 255.0, gamma / inimage->gamma));
				ored[x] = val;
				val = outimage->rgb.green[x] >> 8;
				val = (int) (0.5 + 255 * pow(val / 255.0, inimage->gamma / gamma));
				val = (int) (0.5 + 255 * pow(val / 255.0, gamma / inimage->gamma));
				ogreen[x] = val;
				val = outimage->rgb.blue[x] >> 8;
				val = (int) (0.5 + 255 * pow(val / 255.0, inimage->gamma / gamma));
				val = (int) (0.5 + 255 * pow(val / 255.0, gamma / inimage->gamma));
				oblue[x] = val;
			}

		switch (inimage->type) {
		case IRGB:
			{
				Intensity *red = inimage->rgb.red, *green = inimage->rgb.green,
				*blue = inimage->rgb.blue;
				if (outimage->pixlen == 1 && inimage->pixlen == 1) {	/* most common */
					for (x = inimage->width, ip = epixel; x > 0; x--, pixel++) {
						*ip++ = red[*pixel] >> 8;;
						*ip++ = green[*pixel] >> 8;;
						*ip++ = blue[*pixel] >> 8;;
					}
					for (y = inimage->height; y > 0; y--) {
						if (y == 1)	/* protect last line */
							pixel = inimage->data;
						for (x = inimage->width, ip = epixel, nextr = *ip, *ip++ = 0,
						     nextg = *ip, *ip++ = 0, nextb = *ip, *ip++ = 0; x > 0; x--) {
							register unsigned long rgbindex;
							register int rval,
							 gval, bval,
							 color;

							rgbindex = ((rval = sat[nextr]) & 0xf8) << (Bits + Bits - cBits);
							rgbindex |= ((gval = sat[nextg]) & 0xf8) << (Bits - cBits);
							rgbindex |= (bval = sat[nextb]) >> cBits;
							*dpixel = color = rgbmap[rgbindex];
							if (color == 0xffff)	/* Create nearest color mapping on demand */
								*dpixel = color = rgbmap[rgbindex] = find_nearest(rval, gval, bval, nna, outimage);
							dpixel++;
							rval -= ored[color];
							nextr = *ip++ + err7[rval];	/* this line, next pixel */
							ip[-1] = err1[rval];	/* next line, next pixel */
							ip[-4] += (red[*pixel] >> 8) + err5[rval];	/* next line, this pixel */
							ip[-7] += err3[rval];	/* next line, last pixel */
							gval -= ogreen[color];
							nextg = *ip++ + err7[gval];	/* this line, next pixel */
							ip[-1] = err1[gval];	/* next line, next pixel */
							ip[-4] += (green[*pixel] >> 8) + err5[gval];	/* next line, this pixel */
							ip[-7] += err3[gval];	/* next line, last pixel */
							bval -= oblue[color];
							nextb = *ip++ + err7[bval];	/* this line, next pixel */
							ip[-1] = err1[bval];	/* next line, next pixel */
							ip[-4] += (blue[*pixel++] >> 8) + err5[bval];	/* next line, this pixel */
							ip[-7] += err3[bval];	/* next line, last pixel */
						}
					}
				} else {	/* less common */
					unsigned long temp;
					for (x = inimage->width, ip = epixel; x > 0; x--, pixel += inimage->pixlen) {
						temp = memToVal(pixel, inimage->pixlen);
						*ip++ = red[temp] >> 8;;
						*ip++ = green[temp] >> 8;;
						*ip++ = blue[temp] >> 8;;
					}
					for (y = inimage->height; y > 0; y--) {
						if (y == 1)	/* protect last line */
							pixel = inimage->data;
						for (x = inimage->width, ip = epixel, nextr = *ip, *ip++ = 0,
						     nextg = *ip, *ip++ = 0, nextb = *ip, *ip++ = 0; x > 0; x--) {
							register unsigned long rgbindex;
							register int rval,
							 gval, bval,
							 color;

							rgbindex = ((rval = sat[nextr]) & 0xf8) << (Bits + Bits - cBits);
							rgbindex |= ((gval = sat[nextg]) & 0xf8) << (Bits - cBits);
							rgbindex |= (bval = sat[nextb]) >> cBits;
							color = rgbmap[rgbindex];
							if (color == 0xffff)	/* Create nearest color mapping on demand */
								color = rgbmap[rgbindex] = find_nearest(rval, gval, bval, nna, outimage);
							valToMem(color, dpixel, outimage->pixlen);
							dpixel += outimage->pixlen;
							temp = memToVal(pixel, inimage->pixlen);
							pixel += inimage->pixlen;
							rval -= ored[color];
							nextr = *ip++ + err7[rval];	/* this line, next pixel */
							ip[-1] = err1[rval];	/* next line, next pixel */
							ip[-4] += (red[temp] >> 8) + err5[rval];	/* next line, this pixel */
							ip[-7] += err3[rval];	/* next line, last pixel */
							gval -= ogreen[color];
							nextg = *ip++ + err7[gval];	/* this line, next pixel */
							ip[-1] = err1[gval];	/* next line, next pixel */
							ip[-4] += (green[temp] >> 8) + err5[gval];	/* next line, this pixel */
							ip[-7] += err3[gval];	/* next line, last pixel */
							bval -= oblue[color];
							nextb = *ip++ + err7[bval];	/* this line, next pixel */
							ip[-1] = err1[bval];	/* next line, next pixel */
							ip[-4] += (blue[temp] >> 8) + err5[bval];	/* next line, this pixel */
							ip[-7] += err3[bval];	/* next line, last pixel */
						}
					}
				}
			}
			break;

		case ITRUE:
			if (inimage->pixlen == 3 && outimage->pixlen == 1) {	/* common */
				for (x = inimage->width * 3, ip = epixel; x > 0; x--, ip++, pixel++) {
					*ip = *pixel;
				}
				for (y = inimage->height; y > 0; y--) {
					if (y == 1)	/* protect last line */
						pixel = inimage->data;
					for (x = inimage->width, ip = epixel, nextr = *ip, *ip++ = 0,
					     nextg = *ip, *ip++ = 0, nextb = *ip, *ip++ = 0; x > 0; x--) {
						register unsigned long rgbindex;
						register int rval, gval,
						 bval, color;

						rgbindex = ((rval = sat[nextr]) & 0xf8) << (Bits + Bits - cBits);
						rgbindex |= ((gval = sat[nextg]) & 0xf8) << (Bits - cBits);
						rgbindex |= (bval = sat[nextb]) >> cBits;
						*dpixel = color = rgbmap[rgbindex];
						if (color == 0xffff)	/* Create nearest color mapping on demand */
							*dpixel = color = rgbmap[rgbindex] = find_nearest(rval, gval, bval, nna, outimage);
						dpixel++;
						rval -= ored[color];
						nextr = *ip++ + err7[rval];	/* this line, next pixel */
						ip[-1] = err1[rval];	/* next line, next pixel */
						ip[-4] += *pixel++ + err5[rval];	/* next line, this pixel */
						ip[-7] += err3[rval];	/* next line, last pixel */
						gval -= ogreen[color];
						nextg = *ip++ + err7[gval];	/* this line, next pixel */
						ip[-1] = err1[gval];	/* next line, next pixel */
						ip[-4] += *pixel++ + err5[gval];	/* next line, this pixel */
						ip[-7] += err3[gval];	/* next line, last pixel */
						bval -= oblue[color];
						nextb = *ip++ + err7[bval];	/* this line, next pixel */
						ip[-1] = err1[bval];	/* next line, next pixel */
						ip[-4] += *pixel++ + err5[bval];	/* next line, this pixel */
						ip[-7] += err3[bval];	/* next line, last pixel */
					}
				}
			} else {	/* less common */
				unsigned long temp;
				for (x = inimage->width, ip = epixel; x > 0; x--, pixel += inimage->pixlen) {
					temp = memToVal(pixel, inimage->pixlen);
					*ip++ = TRUE_RED(temp);
					*ip++ = TRUE_GREEN(temp);
					*ip++ = TRUE_BLUE(temp);
				}
				for (y = inimage->height; y > 0; y--) {
					if (y == 1)	/* protect last line */
						pixel = inimage->data;
					for (x = inimage->width, ip = epixel, nextr = *ip, *ip++ = 0,
					     nextg = *ip, *ip++ = 0, nextb = *ip, *ip++ = 0; x > 0; x--) {
						register unsigned long rgbindex;
						register int rval, gval,
						 bval, color;

						rgbindex = ((rval = sat[nextr]) & 0xf8) << (Bits + Bits - cBits);
						rgbindex |= ((gval = sat[nextg]) & 0xf8) << (Bits - cBits);
						rgbindex |= (bval = sat[nextb]) >> cBits;
						color = rgbmap[rgbindex];
						if (color == 0xffff)	/* Create nearest color mapping on demand */
							color = rgbmap[rgbindex] = find_nearest(rval, gval, bval, nna, outimage);
						valToMem(color, dpixel, outimage->pixlen);
						dpixel += outimage->pixlen;
						temp = memToVal(pixel, inimage->pixlen);
						pixel += inimage->pixlen;
						rval -= ored[color];
						nextr = *ip++ + err7[rval];	/* this line, next pixel */
						ip[-1] = err1[rval];	/* next line, next pixel */
						ip[-4] += err5[rval]	/* next line, this pixel */
						    +TRUE_RED(temp);
						ip[-7] += err3[rval];	/* next line, last pixel */
						gval -= ogreen[color];
						nextg = *ip++ + err7[gval];	/* this line, next pixel */
						ip[-1] = err1[gval];	/* next line, next pixel */
						ip[-4] += err5[gval]	/* next line, this pixel */
						    +TRUE_GREEN(temp);
						ip[-7] += err3[gval];	/* next line, last pixel */
						bval -= oblue[color];
						nextb = *ip++ + err7[bval];	/* this line, next pixel */
						ip[-1] = err1[bval];	/* next line, next pixel */
						ip[-4] += err5[bval]	/* next line, this pixel */
						    +TRUE_BLUE(temp);
						ip[-7] += err3[bval];	/* next line, last pixel */
					}
				}
			}
			break;
		}

		lfree((byte *) ored);
		lfree((byte *) ogreen);
		lfree((byte *) oblue);
		lfree((byte *) (epixel - 3));

		/* Clean up nn stuff */
		for (x = NNmaxI * NNmaxI * NNmaxI, nnp = nna; x > 0; x--, nnp++) {
			if (nnp->length != 0) {
				/* free red, green and blue */
				lfree((byte *) nnp->red);
				lfree((byte *) nnp->pixel);
			}
		}
		lfree((char *) nna);
	}
}

/* expand an image into a true color image */
Image *expandtotrue(Image *image)
{
	Pixel fg, bg;
	Image *new_image;
	int x, y;
	byte *spixel, *dpixel, *line;
	unsigned int linelen;
	byte mask;

	CURRFUNC("expandtotrue");
	if TRUEP
		(image)
		    return (image);

	new_image = newTrueImage(image->width, image->height);
	new_image->title = dupString(image->title);
	new_image->gamma = image->gamma;

	switch (image->type) {
	case IBITMAP:
		fg = RGB_TO_TRUE(image->rgb.red[1], image->rgb.green[1], image->rgb.blue[1]);
		bg = RGB_TO_TRUE(image->rgb.red[0], image->rgb.green[0], image->rgb.blue[0]);
		line = image->data;
		dpixel = new_image->data;
		linelen = (image->width / 8) + (image->width % 8 ? 1 : 0);
		for (y = 0; y < image->height; y++) {
			spixel = line;
			mask = 0x80;
			if (new_image->pixlen == 3)	/* most common */
				for (x = 0; x < image->width; x++) {
					valToMem((mask & *spixel ? fg : bg), dpixel, 3);
					mask >>= 1;
					if (!mask) {
						mask = 0x80;
						spixel++;
					}
					dpixel += 3;
			} else	/* less common */
				for (x = 0; x < image->width; x++) {
					valToMem((mask & *spixel ? fg : bg), dpixel, new_image->pixlen);
					mask >>= 1;
					if (!mask) {
						mask = 0x80;
						spixel++;
					}
					dpixel += new_image->pixlen;
				}
			line += linelen;
		}
		new_image->gamma = 1.0;		/* will be linear now */
		break;
	case IRGB:
		spixel = image->data;
		dpixel = new_image->data;
		if (image->pixlen == 1 && new_image->pixlen == 3)	/* most common */
			for (y = 0; y < image->height; y++)
				for (x = 0; x < image->width; x++) {
					register unsigned long temp;
					temp = memToVal(spixel, 1);
					temp = RGB_TO_TRUE(image->rgb.red[temp],
						  image->rgb.green[temp],
						  image->rgb.blue[temp]);
					valToMem(temp, dpixel, 3);
					spixel += 1;
					dpixel += 3;
		} else		/* less common */
			for (y = 0; y < image->height; y++)
				for (x = 0; x < image->width; x++) {
					register unsigned long temp;
					temp = memToVal(spixel, image->pixlen);
					temp = RGB_TO_TRUE(image->rgb.red[temp],
						  image->rgb.green[temp],
						  image->rgb.blue[temp]);
					valToMem(temp, dpixel, new_image->pixlen);
					spixel += image->pixlen;
					dpixel += new_image->pixlen;
				}
		break;
	}
	return (new_image);
}

/* expand a bitmap image into an indexed RGB pixmap */
Image *expandbittoirgb(Image *image, int depth)
{
	Image *new_image;
	int x, y;
	byte *spixel, *dpixel, *line;
	unsigned int linelen;
	byte mask;

	CURRFUNC("expandbittoirgb");
	if (!BITMAPP(image))
		return (image);

	new_image = newRGBImage(image->width, image->height, depth);
	new_image->title = dupString(image->title);
	new_image->gamma = image->gamma;

	new_image->rgb.red[0] = image->rgb.red[0];
	new_image->rgb.green[0] = image->rgb.green[0];
	new_image->rgb.blue[0] = image->rgb.blue[0];
	new_image->rgb.red[1] = image->rgb.red[1];
	new_image->rgb.green[1] = image->rgb.green[1];
	new_image->rgb.blue[1] = image->rgb.blue[1];
	new_image->rgb.used = 2;
	line = image->data;
	dpixel = new_image->data;
	linelen = (image->width / 8) + (image->width % 8 ? 1 : 0);
	for (y = 0; y < image->height; y++) {
		spixel = line;
		mask = 0x80;
		if (new_image->pixlen == 1)	/* most common */
			for (x = 0; x < image->width; x++) {
				valToMem((mask & *spixel ? 1 : 0), dpixel, 1);
				mask >>= 1;
				if (!mask) {
					mask = 0x80;
					spixel++;
				}
				dpixel += 1;
		} else		/* less common */
			for (x = 0; x < image->width; x++) {
				valToMem((mask & *spixel ? 1 : 0), dpixel, new_image->pixlen);
				mask >>= 1;
				if (!mask) {
					mask = 0x80;
					spixel++;
				}
				dpixel += new_image->pixlen;
			}
		line += linelen;
	}
	return (new_image);
}

/* expand an indexed RGB pixmaps depth */
Image *expandirgbdepth(Image *image, int depth)
{
	Image *new_image;
	int i, x, y;
	byte *spixel, *dpixel;

	CURRFUNC("expandirgb");
	if (TRUEP(image))
		return (image);
	if (BITMAPP(image))
		return expandbittoirgb(image, depth);

	if (((depth + 7) / 8) == image->pixlen) {	/* no change in storage */
		image->depth = depth;
		resizeRGBMapData(&(image->rgb), depthToColors(depth));
	}
	/* change in storage size */
	new_image = newRGBImage(image->width, image->height, depth);
	new_image->title = dupString(image->title);
	new_image->gamma = image->gamma;

	/* copy the color map */
	for (i = 0; i < image->rgb.used; i++) {
		new_image->rgb.red[i] = image->rgb.red[i];
		new_image->rgb.green[i] = image->rgb.green[i];
		new_image->rgb.blue[i] = image->rgb.blue[i];
	}
	new_image->rgb.used = image->rgb.used;

	spixel = image->data;
	dpixel = new_image->data;
	if (image->pixlen == 1 && new_image->pixlen == 2)	/* most common */
		for (y = 0; y < image->height; y++)
			for (x = 0; x < image->width; x++) {
				*dpixel = 0;	/* sign extend bigendian 8bit to 16 bit */
				*(dpixel + 1) = *spixel;
				spixel += 1;
				dpixel += 2;
	} else			/* less common */
		for (y = 0; y < image->height; y++)
			for (x = 0; x < image->width; x++) {
				register unsigned long temp;
				temp = memToVal(spixel, image->pixlen);
				valToMem(temp, dpixel, new_image->pixlen);
				spixel += image->pixlen;
				dpixel += new_image->pixlen;
			}
	return (new_image);
}
