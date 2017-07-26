/* new.c:
 *
 * functions to allocate and deallocate structures and structure data
 *
 * jim frost 09.29.89
 *
 * Copyright 1989, 1991 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */

#include "copyright.h"
#include "xli.h"

/* this table is useful for quick conversions between depth and ncolors */

unsigned long DepthToColorsTable[] =
{
  /*  0 */ 1,
  /*  1 */ 2,
  /*  2 */ 4,
  /*  3 */ 8,
  /*  4 */ 16,
  /*  5 */ 32,
  /*  6 */ 64,
  /*  7 */ 128,
  /*  8 */ 256,
  /*  9 */ 512,
  /* 10 */ 1024,
  /* 11 */ 2048,
  /* 12 */ 4096,
  /* 13 */ 8192,
  /* 14 */ 16384,
  /* 15 */ 32768,
  /* 16 */ 65536,
  /* 17 */ 131072,
  /* 18 */ 262144,
  /* 19 */ 524288,
  /* 20 */ 1048576,
  /* 21 */ 2097152,
  /* 22 */ 4194304,
  /* 23 */ 8388608,
  /* 24 */ 16777216,
  /* 25 */ 33554432,
  /* 26 */ 67108864,
  /* 27 */ 134217728,
  /* 28 */ 268435456,
  /* 29 */ 536870912,
  /* 30 */ 1073741824,
  /* 31 */ 2147483648U,

  /* bigger than unsigned int; this is good enough */
  /* 32 */ 2147483648U
};

unsigned long colorsToDepth(long unsigned int ncolors)
{
	unsigned long a;

	for (a = 0; (a < 32) && (DepthToColorsTable[a] < ncolors); a++)
		/* EMPTY */
		;
	return (a);
}

char *dupString(char *s)
{
	char *d;

	if (!s)
		return (NULL);
	d = (char *) lmalloc(strlen(s) + 1);
	strcpy(d, s);
	return (d);
}

void newRGBMapData(RGBMap *rgb, unsigned int size)
{
	CURRFUNC("newRGBMapData");
	rgb->used = 0;
	rgb->size = size;
	rgb->compressed = FALSE;
	rgb->red = (Intensity *) lmalloc(sizeof(Intensity) * size);
	rgb->green = (Intensity *) lmalloc(sizeof(Intensity) * size);
	rgb->blue = (Intensity *) lmalloc(sizeof(Intensity) * size);
}

void resizeRGBMapData(RGBMap *rgb, unsigned int size)
{
	CURRFUNC("resizeRGBMapData");
	rgb->red = (Intensity *) lrealloc((byte *) rgb->red,
		sizeof(Intensity) * size);
	rgb->green = (Intensity *) lrealloc((byte *) rgb->green,
		sizeof(Intensity) * size);
	rgb->blue = (Intensity *) lrealloc((byte *) rgb->blue,
		sizeof(Intensity) * size);
	rgb->size = size;
}

void freeRGBMapData(RGBMap *rgb)
{
	CURRFUNC("freeRGBMapData");
	lfree((byte *) rgb->red);
	lfree((byte *) rgb->green);
	lfree((byte *) rgb->blue);
}

static unsigned int ovmul(unsigned int a, unsigned int b)
{
	unsigned int r;

	r = a * b;
	if (r / a != b) {
		memoryExhausted();
	}

	return r;
}

static Image *newImage(unsigned width, unsigned height)
{
	Image *image;

	CURRFUNC("newImage");
	image = (Image *) lmalloc(sizeof(Image));
	image->title = 0;
	image->width = width;
	image->height = height;
	image->gamma = UNSET_GAMMA;
	image->flags = 0;

	return image;
}

Image *newBitImage(unsigned width, unsigned height)
{
	Image *image;
	unsigned int linelen;

	CURRFUNC("newBitImage");
	image = newImage(width, height);
	image->type = IBITMAP;
	newRGBMapData(&(image->rgb), (unsigned int) 2);
	*(image->rgb.red) = *(image->rgb.green) = *(image->rgb.blue) = 65535;
	*(image->rgb.red + 1) = *(image->rgb.green + 1) = *(image->rgb.blue + 1) = 0;
	image->rgb.used = 2;
	image->depth = 1;
	linelen = ((width + 7) / 8);
	image->data = (unsigned char *) lcalloc(ovmul(linelen, height));

	return image;
}

Image *newRGBImage(unsigned int width, unsigned int height, unsigned int depth)
{
	Image *image;
	unsigned int pixlen, numcolors;

	CURRFUNC("newRGBImage");

	/* special case for `zero' depth image, which is sometimes
	 * interpreted as `one color'
	 */
	if (depth == 0)
		depth = 1;
	pixlen = ((depth + 7) / 8);
	numcolors = depthToColors(depth);
	image = newImage(width, height);
	image->type = IRGB;
	newRGBMapData(&(image->rgb), numcolors);
	image->depth = depth;
	image->pixlen = pixlen;
	image->data =
		(unsigned char *) lmalloc(ovmul(ovmul(width, height), pixlen));

	return image;
}

Image *newTrueImage(unsigned int width, unsigned int height)
{
	Image *image;

	CURRFUNC("NewTrueImage");
	image = newImage(width, height);
	image->type = ITRUE;
	image->rgb.used = image->rgb.size = 0;
	image->depth = 24;
	image->pixlen = 3;
	image->data =
		(unsigned char *) lmalloc(ovmul(ovmul(width, height), 3));

	return image;
}

void freeImageData(Image *image)
{
	if (image->title) {
		lfree((byte *) image->title);
		image->title = NULL;
	}
	if (!TRUEP(image))
		freeRGBMapData(&(image->rgb));
	lfree(image->data);
}

void freeImage(Image *image)
{
	freeImageData(image);
	lfree((byte *) image);
}

byte *lmalloc(unsigned int size)
{
	byte *area;

	if (size == 0) {
		size = 1;
		if (globals._Xdebug)
			fprintf(stderr, "lmalloc given zero size!\n");
	}
	if (!(area = (byte *) malloc(size))) {
		memoryExhausted();
		/* NOTREACHED */
	}
	return (area);
}

byte *lcalloc(unsigned int size)
{
	byte *area;

	if (size == 0) {
		size = 1;
		if (globals._Xdebug)
			fprintf(stderr, "lcalloc given zero size!\n");
	}
	if (!(area = (byte *) calloc(1, size))) {
		memoryExhausted();
		/* NOTREACHED */
	}
	return (area);
}

byte *lrealloc(byte *old, unsigned int size)
{
	byte *area;

	if (size == 0) {
		size = 1;
		if (globals._Xdebug)
			fprintf(stderr, "lrealloc given zero size!\n");
	}
	if (!(area = (byte *) realloc(old, size))) {
		memoryExhausted();
		/* NOTREACHED */
	}
	return (area);
}

void lfree(byte *area)
{
	free(area);
}
