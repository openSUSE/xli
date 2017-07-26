/* zoom.c:
 *
 * zoom an image
 *
 * jim frost 10.11.89
 *
 * Copyright 1989 Jim Frost.  See included file "copyright.h" for complete
 * copyright information.
 */

#include "copyright.h"
#include "xli.h"

static unsigned int *buildIndex(unsigned int width, unsigned int zoom, unsigned int *rwidth)
{
	unsigned int *index;
	unsigned int	a;

	if (!zoom) {
		zoom = 100;
	}
	*rwidth = width * zoom / 100;
	index= (unsigned int *)lmalloc(sizeof(unsigned int) * *rwidth);
	for (a = 0; a < *rwidth; a++)
		*(index + a) = a * (width - 1) / (*rwidth - 1);
		
	return(index);
}

Image *zoom(Image *oimage, unsigned int xzoom, unsigned int yzoom, boolean verbose, boolean changetitle)
{ char          buf[BUFSIZ];
  float         gamma;
  Image        *image;
  unsigned int *xindex, *yindex;
  unsigned int  xwidth, ywidth;
  unsigned int  x, y, xsrc, ysrc;
  unsigned int  pixlen;
  unsigned int  srclinelen;
  unsigned int  destlinelen;
  byte         *srcline, *srcptr;
  byte         *destline, *destptr;
  byte          srcmask, destmask, bit;
  Pixel         value;

  CURRFUNC("zoom");

  image = 0;
  if ((!xzoom || xzoom==100) && (!yzoom || yzoom==100)) 
    return(oimage);

  if (!xzoom) {
    if (verbose)
      printf("  Zooming image Y axis by %d%%...", yzoom);
    if (changetitle)
      snprintf(buf, BUFSIZ, "%s (Y zoom %d%%)", oimage->title, yzoom);
  }
  else if (!yzoom) {
    if (verbose)
      printf("  Zooming image X axis by %d%%...", xzoom);
    if (changetitle)
      snprintf(buf, BUFSIZ, "%s (X zoom %d%%)", oimage->title, xzoom);
  }
  else if (xzoom == yzoom) {
    if (verbose)
      printf("  Zooming image by %d%%...", xzoom);
    if (changetitle)
      snprintf(buf, BUFSIZ, "%s (%d%% zoom)", oimage->title, xzoom);
  }
  else {
    if (verbose)
      printf("  Zooming image X axis by %d%% and Y axis by %d%%...",
	     xzoom, yzoom);
    if (changetitle)
      snprintf(buf, BUFSIZ, "%s (X zoom %d%% Y zoom %d%%)", oimage->title,
	    xzoom, yzoom);
  }
  buf[BUFSIZ-1] = '\0';
  if (!changetitle)
    strcpy(buf,oimage->title);

  if (verbose)
    fflush(stdout);
  gamma= oimage->gamma;

  xindex= buildIndex(oimage->width, xzoom, &xwidth);
  yindex= buildIndex(oimage->height, yzoom, &ywidth);

  switch (oimage->type) {
  case IBITMAP:
    image= newBitImage(xwidth, ywidth);
    for (x= 0; x < oimage->rgb.used; x++) {
      *(image->rgb.red + x)= *(oimage->rgb.red + x);
      *(image->rgb.green + x)= *(oimage->rgb.green + x);
      *(image->rgb.blue + x)= *(oimage->rgb.blue + x);
    }
    image->rgb.used= oimage->rgb.used;
    destline= image->data;
    destlinelen= (xwidth / 8) + (xwidth % 8 ? 1 : 0);
    srcline= oimage->data;
    srclinelen= (oimage->width / 8) + (oimage->width % 8 ? 1 : 0);
    for (y= 0, ysrc= *(yindex + y); y < ywidth; y++) {
      while (ysrc != *(yindex + y)) {
	ysrc++;
	srcline += srclinelen;
      }
      srcptr= srcline;
      destptr= destline;
      srcmask= 0x80;
      destmask= 0x80;
      bit= srcmask & *srcptr;
      for (x= 0, xsrc= *(xindex + x); x < xwidth; x++) {
	if (xsrc != *(xindex + x)) {
	  do {
	    xsrc++;
	    if (!(srcmask >>= 1)) {
	      srcmask= 0x80;
	      srcptr++;
	    }
	  } while (xsrc != *(xindex + x));
	  bit= srcmask & *srcptr;
	}
	if (bit)
	  *destptr |= destmask;
	if (!(destmask >>= 1)) {
	  destmask= 0x80;
	  destptr++;
	}
      }
      destline += destlinelen;
    }
    break;

  case IRGB:
    image= newRGBImage(xwidth, ywidth, oimage->depth);
    for (x= 0; x < oimage->rgb.used; x++) {
      *(image->rgb.red + x)= *(oimage->rgb.red + x);
      *(image->rgb.green + x)= *(oimage->rgb.green + x);
      *(image->rgb.blue + x)= *(oimage->rgb.blue + x);
    }
    image->rgb.used= oimage->rgb.used;

    pixlen= oimage->pixlen;
    destptr= image->data;
    srcline= oimage->data;
    srclinelen= oimage->width * pixlen;
    for (y= 0, ysrc= *(yindex + y); y < ywidth; y++) {
      while (ysrc != *(yindex + y)) {
	ysrc++;
	srcline += srclinelen;
      }

      srcptr= srcline;
      value= memToVal(srcptr, image->pixlen);
      if(pixlen == 1 && image->pixlen == 1)	/* common */
        for (x= 0, xsrc= *(xindex + x); x < xwidth; x++) {
          if (xsrc != *(xindex + x)) {
            do {
              xsrc++;
              srcptr += 1;
            } while (xsrc != *(xindex + x));
            value= memToVal(srcptr, 1);
          }
          valToMem(value, destptr, 1);
          destptr += 1;
        }
      else	/* less common */
        for (x= 0, xsrc= *(xindex + x); x < xwidth; x++) {
          if (xsrc != *(xindex + x)) {
            do {
              xsrc++;
              srcptr += image->pixlen;
            } while (xsrc != *(xindex + x));
            value= memToVal(srcptr, image->pixlen);
          }
          valToMem(value, destptr, pixlen);
          destptr += pixlen;
        }
    }
    break;

  case ITRUE:
    if (!RGBP(oimage))
      image= newTrueImage(xwidth, ywidth);
    pixlen= oimage->pixlen;
    destptr= image->data;
    srcline= oimage->data;
    srclinelen= oimage->width * pixlen;
    for (y= 0, ysrc= *(yindex + y); y < ywidth; y++) {
      while (ysrc != *(yindex + y)) {
	ysrc++;
	srcline += srclinelen;
      }

      srcptr= srcline;
      value= memToVal(srcptr, image->pixlen);
      if(pixlen == 3 && image->pixlen == 3)	/* common */
        for (x= 0, xsrc= *(xindex + x); x < xwidth; x++) {
          if (xsrc != *(xindex + x)) {
            do {
              xsrc++;
              srcptr += 3;
            } while (xsrc != *(xindex + x));
            value= memToVal(srcptr, 3);
          }
          valToMem(value, destptr, 3);
          destptr += 3;
        }
      else	/* less common */
        for (x= 0, xsrc= *(xindex + x); x < xwidth; x++) {
          if (xsrc != *(xindex + x)) {
            do {
              xsrc++;
              srcptr += image->pixlen;
            } while (xsrc != *(xindex + x));
            value= memToVal(srcptr, image->pixlen);
          }
          valToMem(value, destptr, pixlen);
          destptr += pixlen;
        }
    }
    break;
  }

  image->title = dupString(buf);
  image->gamma= gamma;
  lfree((byte *)xindex);
  lfree((byte *)yindex);
  if (verbose)
    printf("done\n");
  return(image);
}
