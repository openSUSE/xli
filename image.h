/* image.h:
 * portable image type declarations
 *
 * jim frost 10.02.89
 *
 * Copyright 1989 Jim Frost.  See included file "copyright.h" for complete
 * copyright information.
 */

#include "copyright.h"

typedef struct rgbmap {
	unsigned int size;	/* size of RGB map */
	unsigned int used;	/* number of colors used in RGB map */
	boolean compressed;	/* image uses colormap fully */
	Intensity *red;		/* color values in X style */
	Intensity *green;
	Intensity *blue;
} RGBMap;

/* image structure */

typedef struct {
	char *title;		/* name of image */
	unsigned int type;	/* type of image */
	RGBMap rgb;		/* RGB map of image if IRGB type */
	unsigned int width;	/* width of image in pixels */
	unsigned int height;	/* height of image in pixels */
	unsigned int depth;	/* depth of image in bits if IRGB type */
	unsigned int pixlen;	/* length of pixel if IRGB type */
	byte *data;		/* data rounded to full byte for each row */
	float gamma;		/* gamma of display the image is adjusted for */
	unsigned long flags;	/* sundry flags */
} Image;

#define IBITMAP 0		/* image is a bitmap */
#define IRGB    1		/* image is RGB */
#define ITRUE   2		/* image is true color */

#define BITMAPP(IMAGE) ((IMAGE)->type == IBITMAP)
#define RGBP(IMAGE)    ((IMAGE)->type == IRGB)
#define TRUEP(IMAGE)   ((IMAGE)->type == ITRUE)

#define TRUE_RED(PIXVAL)   (((unsigned long)((PIXVAL) & 0xff0000)) >> 16)
#define TRUE_GREEN(PIXVAL) (((unsigned long)((PIXVAL) & 0xff00)) >> 8)
#define TRUE_BLUE(PIXVAL)   ((unsigned long)((PIXVAL) & 0xff))
#define RGB_TO_TRUE(R,G,B) \
  ((((unsigned long)((R) & 0xff00)) << 8) | ((G) & 0xff00) | (((unsigned short)(B)) >> 8))

#define FLAG_ISCALE 1		/* image scaled by decoder */

#define UNSET_GAMMA 0.0

#define depthToColors(n) DepthToColorsTable[((n) < 32 ? (n) : 32)]

/* this returns the (approximate) intensity of an RGB triple */

#define colorIntensity(R,G,B) \
	(RedIntensity[((unsigned short)(R)) >> 8] \
	+ GreenIntensity[((unsigned short)(G)) >> 8] \
	+ BlueIntensity[((unsigned short)(B)) >> 8])

extern unsigned short RedIntensity[];
extern unsigned short GreenIntensity[];
extern unsigned short BlueIntensity[];

/* Architecture independent memory to value conversions.
 * Note the "Normal" internal format is big endian.
 */

#define memToVal(PTR,LEN) (\
	(LEN) == 1 ? (unsigned long)(*((byte *)(PTR))) : \
	(LEN) == 2 ? (unsigned long)(((unsigned long)(*((byte *)(PTR))))<< 8) \
		+ (*(((byte *)(PTR))+1)) : \
	(LEN) == 3 ? (unsigned long)(((unsigned long)(*((byte *)(PTR))))<<16) \
		+ (((unsigned long)(*(((byte *)(PTR))+1)))<< 8) \
		+ (*(((byte *)(PTR))+2)) : \
	(unsigned long)(((unsigned long)(*((byte *)(PTR))))<<24) \
		+ (((unsigned long)(*(((byte *)(PTR))+1)))<<16) \
		+ (((unsigned long)(*(((byte *)(PTR))+2)))<< 8) \
		+ (*(((byte *)(PTR))+3)))

#define valToMem(VAL,PTR,LEN)  ( \
(LEN) == 1 ? (*((byte *)(PTR)) = (VAL)) : \
(LEN) == 2 ? (*((byte *)(PTR)) = (((unsigned long)(VAL))>> 8), \
	*(((byte *)(PTR))+1) = (VAL)) : \
(LEN) == 3 ? (*((byte *)(PTR)) = (((unsigned long)(VAL))>>16), \
	*(((byte *)(PTR))+1) = (((unsigned long)(VAL))>> 8), \
	*(((byte *)(PTR))+2) = (VAL)) : \
	(*((byte *)(PTR)) = (((unsigned long)(VAL))>>24), \
	*(((byte *)(PTR))+1) = (((unsigned long)(VAL))>>16), \
	*(((byte *)(PTR))+2) = (((unsigned long)(VAL))>> 8), \
	*(((byte *)(PTR))+3) = (VAL)))

#define memToValLSB(PTR,LEN) ( \
	(LEN) == 1 ? (unsigned long)(*((byte *)(PTR))) : \
	(LEN) == 2 ? (unsigned long)(*((byte *)(PTR))) \
		+ (((unsigned long)(*(((byte *)(PTR))+1)))<< 8) : \
	(LEN) == 3 ? (unsigned long)(*((byte *)(PTR))) \
		+ (((unsigned long)(*(((byte *)(PTR))+1)))<< 8) \
		+ (((unsigned long)(*(((byte *)(PTR))+2)))<<16) : \
	(unsigned long)(*((byte *)(PTR))) \
		+ (((unsigned long)(*(((byte *)(PTR))+1)))<< 8) \
		+ (((unsigned long)(*(((byte *)(PTR))+2)))<<16) \
		+ (((unsigned long)(*(((byte *)(PTR))+3)))<<24))


/* this is provided for orthagonality */

#define valToMemLSB(VAL,PTR,LEN) ( \
(LEN) == 1 ? (*((byte *)(PTR)) = (VAL)) : \
(LEN) == 2 ? (*((byte *)(PTR)) = (VAL), \
	*(((byte *)(PTR))+1) = (((unsigned long)(VAL))>> 8)) : \
(LEN) == 3 ? (*((byte *)(PTR)) = (VAL), \
	*(((byte *)(PTR))+1) = (((unsigned long)(VAL))>> 8), \
	*(((byte *)(PTR))+2) = (((unsigned long)(VAL))>>16)) : \
	(*((byte *)(PTR)) = (VAL), \
	*(((byte *)(PTR))+1) = (((unsigned long)(VAL))>> 8), \
	*(((byte *)(PTR))+2) = (((unsigned long)(VAL))>>16), \
	*(((byte *)(PTR))+3) = (((unsigned long)(VAL))>>24)))
