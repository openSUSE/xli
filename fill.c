/* #ident	"@(#)x11:contrib/clients/xloadimage/fill.c 6.8 93/07/23 Labtam" */
/* fill.c:
 *
 * fill an image area with a particular pixel value
 *
 * jim frost 10.02.89
 *
 * Copyright 1989, 1991 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */

#include "copyright.h"
#include "xli.h"

void fill(Image *image, unsigned int fx, unsigned int fy, unsigned int fw, unsigned int fh, Pixel pixval)
{ unsigned int  x, y;
  unsigned int  linelen, start;
  byte         *lineptr, *pixptr;
  byte          startmask, mask;

  switch(image->type) {
  case IBITMAP:

    /* this could be made a lot faster
     */

    linelen= (image->width / 8) + (image->width % 8 ? 1 : 0);
    lineptr= image->data + (linelen * fy);
    start= (fx / 8);
    startmask= 0x80 >> (fx % 8);
    for (y= 0; y < fh; y++) {
      mask= startmask;
      pixptr= lineptr + start;
      if (pixval)
	for (x= 0; x < fw; x++) {
	  *pixptr |= mask;
	  mask >>= 1;
	  if (mask == 0) {
	    mask= 0x80;
	    pixptr++;
	  }
	}
      else
	for (x= 0; x < fw; x++) {
	  *pixptr &= ~mask;
	  mask >>= 1;
	  if (mask == 0) {
	    mask= 0x80;
	    pixptr++;
	  }
	}
      lineptr += linelen;
    }
    break;

  case IRGB:
  case ITRUE:
    linelen= image->width * image->pixlen;
    start= image->pixlen * fx;
    lineptr= image->data + (linelen * fy);
    for (y= 0; y < fh; y++) {
      pixptr= lineptr + start;
      for (x= 0; x < fw; x++) {
	valToMem(pixval, pixptr, image->pixlen);
	pixptr += image->pixlen;
      }
      lineptr += linelen;
    }
    break;
  default:
    printf("fill: Unsupported image type (ignored)\n");
    return;
  }
}
