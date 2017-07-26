/* merge.c:
 *
 * merge two images
 *
 * jim frost 09.27.89
 *
 * Copyright 1989, 1990, 1991 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */

#include "copyright.h"
#include "xli.h"

/* if merging bitmaps they don't have to be converted to
 * 24-bit.  this saves a lot of space.
 */

static Image *bitmapToBitmap(Image *src, Image *dst, unsigned int atx, unsigned int aty, unsigned int clipw, unsigned int cliph)
{ unsigned int  dstlinelen, srclinelen;
  unsigned int  dststart;
  unsigned int  flip;
  unsigned int  x, y;
  byte         *dstline, *srcline;
  byte          dststartmask;
  byte          dstmask, srcmask;
  byte         *dstpixel, *srcpixel;

  dstlinelen= (dst->width / 8) + (dst->width % 8 ? 1 : 0);
  srclinelen= (src->width / 8) + (src->width % 8 ? 1 : 0);
  dstline= dst->data + (aty * dstlinelen);
  srcline= src->data;
  dststart= atx / 8;
  dststartmask= 0x80 >> (atx % 8);
  flip= ((*dst->rgb.red == *(src->rgb.red + 1)) &&
	 (*dst->rgb.green == *(src->rgb.green + 1)) &&
	 (*dst->rgb.blue == *(src->rgb.blue + 1)));
  for (y= 0; y < cliph; y++) {
    dstpixel= dstline + dststart;
    srcpixel= srcline;
    dstmask= dststartmask;
    srcmask= 0x80;
    for (x= 0; x < clipw; x++) {
      if (flip)
	if (*srcpixel & srcmask)
	  *dstpixel &= ~dstmask;
	else
	  *dstpixel |= dstmask;
      else
	if (*srcpixel & srcmask)
	  *dstpixel |= dstmask;
	else
	  *dstpixel &= ~dstmask;
      dstmask >>= 1;
      srcmask >>= 1;
      if (dstmask == 0) {
	dstmask= 0x80;
	dstpixel++;
      }
      if (srcmask == 0) {
	srcmask= 0x80;
	srcpixel++;
      }
    }
    dstline += dstlinelen;
    srcline += srclinelen;
  }
  if (globals.verbose)
    printf("done\n");
  return(dst);
}

#ifndef FABS
#define FABS(x) ((x) < 0.0 ? -(x) : (x))
#endif

/* merge any to true */
static Image *anyToTrue(Image *src, Image *dst,
	unsigned int atx, unsigned int aty,
	unsigned int clipw, unsigned int cliph)
{ Pixel         fg, bg;
  unsigned int  dstlinelen, srclinelen;
  unsigned int  dststart;
  unsigned int  x, y;
  byte         *dstline, *srcline;
  byte         *dstpixel, *srcpixel;
  byte          srcmask;
  Pixel         pixval;

  /* The gamma of our two images just need to be equal. */
  /* Move towards our output gamma to minimize gamma changes */
  if (GAMMA_NOT_EQUAL(src->gamma, dst->gamma)) {
    if (FABS(src->gamma-globals.display_gamma) > FABS(dst->gamma-globals.display_gamma))
      gammacorrect(src, dst->gamma, globals.verbose);
    else
      gammacorrect(dst, src->gamma, globals.verbose);
  }

  switch (src->type) {
  case IBITMAP:
    fg= RGB_TO_TRUE(src->rgb.red[1], src->rgb.green[1], src->rgb.blue[1]);
    bg= RGB_TO_TRUE(src->rgb.red[0], src->rgb.green[0], src->rgb.blue[0]);
    dstlinelen= dst->width * dst->pixlen;
    srclinelen= (src->width / 8) + (src->width % 8 ? 1 : 0);
    dstline= dst->data + (aty * dstlinelen);
    srcline= src->data;
    dststart= atx * dst->pixlen;
    for (y= 0; y < cliph; y++) {
      dstpixel= dstline + dststart;
      srcpixel= srcline;
      srcmask= 0x80;
      if(dst->pixlen == 3)	/* the ususal case */
        for (x= 0; x < clipw; x++) {
          valToMem((*srcpixel & srcmask ? fg : bg), dstpixel, 3);
          dstpixel += 3;
          srcmask >>= 1;
          if (srcmask == 0) {
            srcpixel++;
            srcmask= 0x80;
          }
        }
      else	/* less ususal case */
        for (x= 0; x < clipw; x++) {
          valToMem((*srcpixel & srcmask ? fg : bg), dstpixel, dst->pixlen);
          dstpixel += dst->pixlen;
          srcmask >>= 1;
          if (srcmask == 0) {
            srcpixel++;
            srcmask= 0x80;
          }
        }
      dstline += dstlinelen;
      srcline += srclinelen;
    }
    break;
  
  case IRGB:
    dstlinelen= dst->width * dst->pixlen;
    srclinelen= src->width * src->pixlen;
    dststart= atx * dst->pixlen;
    dstline= dst->data + (aty * dstlinelen);
    srcline= src->data;

    if(src->pixlen == 1)	/* the usual case */
      for (y= 0; y < cliph; y++) {
        dstpixel= dstline + dststart;
        srcpixel= srcline;
        for (x= 0; x < clipw; x++) {
          pixval= memToVal(srcpixel, 1);
          *(dstpixel++)= src->rgb.red[pixval] >> 8;
          *(dstpixel++)= src->rgb.green[pixval] >> 8;
          *(dstpixel++)= src->rgb.blue[pixval] >> 8;
          srcpixel += 1;
        }
        dstline += dstlinelen;
        srcline += srclinelen;
      }
    else	/* the less ususal */
      for (y= 0; y < cliph; y++) {
        dstpixel= dstline + dststart;
        srcpixel= srcline;
        for (x= 0; x < clipw; x++) {
          pixval= memToVal(srcpixel, src->pixlen);
          *(dstpixel++)= src->rgb.red[pixval] >> 8;
          *(dstpixel++)= src->rgb.green[pixval] >> 8;
          *(dstpixel++)= src->rgb.blue[pixval] >> 8;
          srcpixel += src->pixlen;
        }
        dstline += dstlinelen;
        srcline += srclinelen;
      }
    break;

  case ITRUE:
    dstlinelen= dst->width * dst->pixlen;
    srclinelen= src->width * src->pixlen;
    dststart= atx * dst->pixlen;
    dstline= dst->data + (aty * dstlinelen);
    srcline= src->data;

    for (y= 0; y < cliph; y++) {
      dstpixel= dstline + dststart;
      srcpixel= srcline;
      for (x= 0; x < clipw; x++) {
	*(dstpixel++)= *(srcpixel++);
	*(dstpixel++)= *(srcpixel++);
	*(dstpixel++)= *(srcpixel++);
      }
      dstline += dstlinelen;
      srcline += srclinelen;
    }
    break;
  }
  if (globals.verbose)
    printf("done\n");
  return(dst);
}

/* put src image on dst image
 */

Image *merge(Image *idst, Image *isrc, int atx, int aty, ImageOptions *imgopp)
{ int clipw, cliph;
  Image *src = isrc,*dst = idst,*outimage;

  if (globals.verbose) {
    printf("  Merging...");
    fflush(stdout);
  }

  /* adjust clipping of src to fit within dst
   */

  clipw= src->width;
  cliph= src->height;
  if ((atx + clipw < 0) || (aty + cliph < 0) ||
      (atx >= (int)dst->width) ||
      (aty >= (int)dst->height)) /* not on dst, ignore */
    return dst;

  if (atx + clipw > dst->width)
    clipw = dst->width - atx;
  if (aty + cliph > dst->height)
    cliph = dst->height - aty;

  /* extra clipping required for negative offsets
   */

  if ( atx < 0 || aty < 0 ) {
    int clipx, clipy;
    Image *tmp;
 
    if ( atx < 0 ) {
      clipx = -atx;
      clipw += atx;
      atx = 0;
    }
    else
      clipx = 0;
    
    if ( aty < 0 ) {
      clipy = -aty;
      cliph += aty;
      aty = 0;
    }
    else
      clipy = 0;
    
    tmp = clip(src, clipx, clipy, clipw, cliph, imgopp);
    if (src != tmp && src != isrc)	/* free imtermediate, but preserve input */
      freeImage(src);
    src = tmp;
  }
 
  if (BITMAPP(dst) && BITMAPP(src)) {
    outimage= bitmapToBitmap(src, dst, (unsigned int)atx, (unsigned int)aty,
			     clipw, cliph);
  } else {
    if (!TRUEP(dst)) {		/* convert to true */
      Image *tmp;
      tmp = expandtotrue(dst);
      if (dst != tmp && dst != idst)
	freeImage(dst);
     dst = tmp;
    }
    outimage = anyToTrue(src, dst, (unsigned int)atx, (unsigned int)aty,
			  clipw, cliph);
  }
  /* do the right thing */
  if (dst != outimage && dst != idst)
    freeImage(dst);
  if (src != outimage && src != isrc)
    freeImage(src);

  return(outimage);
}
