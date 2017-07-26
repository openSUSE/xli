/* #ident	"@(#)x11:contrib/clients/xloadimage/smooth.c 1.10 94/07/29 Labtam" */
/* smooth.c:
 *
 * this performs a smoothing convolution using a 3x3 area.
 *
 * jim frost 09.20.90
 *
 * Copyright 1990, 1991 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */

#include "copyright.h"
#include "xli.h"

static Image *doSmooth(Image *isrc)
{ Image *src = isrc, *dest, *tmp;
  int    x, y, x1, y1, linelen;
  int    xindex[3];
  byte  *yindex[3];
  byte  *srcptr, *destptr;
  unsigned long avgred, avggreen, avgblue;

  /* build true color image from old image and allocate new image
   */

  tmp= expandtotrue(src);
  if (src != tmp && src != isrc)
    freeImage(src);
  src = tmp;

  dest= newTrueImage(src->width, src->height);
  dest->title= (char *)lmalloc(strlen(src->title) + 12);
  sprintf(dest->title, "%s (smoothed)", src->title);
  dest->gamma= src->gamma;

  /* run through src and take a guess as to what the color should
   * actually be.
   */

  destptr= dest->data;
  linelen= src->pixlen * src->width;
  if(dest->pixlen == 3 && src->pixlen == 3) {	/* usual case */
    for (y= 0; y < src->height; y++) {
      yindex[1]= src->data + (y * linelen);
      yindex[0]= yindex[1] - (y > 0 ? linelen : 0);
      yindex[2]= yindex[1] + (y < src->height - 1 ? linelen : 0);
      for (x= 0; x < src->width; x++) {
        avgred= avggreen= avgblue= 0;
        xindex[1]= x * 3;
        xindex[0]= xindex[1] - (x > 0 ? 3 : 0);
        xindex[2]= xindex[1] + (x < src->width - 1 ? 3 : 0);
        for (y1= 0; y1 < 3; y1++) {
          for (x1= 0; x1 < 3; x1++) {
            srcptr = yindex[y1] + xindex[x1];
            avgred += *srcptr;
            avggreen += *(srcptr + 1);
            avgblue += *(srcptr + 2);
          }
        }
  
        /* average the pixel values
         */
  
        *destptr++ = ((avgred + 8) / 9);
        *destptr++ = ((avggreen + 8) / 9);
        *destptr++ = ((avgblue + 8) / 9);
      }
    }
  } else {	/* less usual */
    Pixel  pixval;
    for (y= 0; y < src->height; y++) {
      yindex[1]= src->data + (y * linelen);
      yindex[0]= yindex[1] - (y > 0 ? linelen : 0);
      yindex[2]= yindex[1] + (y < src->height - 1 ? linelen : 0);
      for (x= 0; x < src->width; x++) {
        avgred= avggreen= avgblue= 0;
        xindex[1]= x * src->pixlen;
        xindex[0]= xindex[1] - (x > 0 ? src->pixlen : 0);
        xindex[2]= xindex[1] + (x < src->width - 1 ? src->pixlen : 0);
        for (y1= 0; y1 < 3; y1++) {
          for (x1= 0; x1 < 3; x1++) {
            pixval= memToVal(yindex[y1] + xindex[x1], src->pixlen);
            avgred += TRUE_RED(pixval);
            avggreen += TRUE_GREEN(pixval);
            avgblue += TRUE_BLUE(pixval);
          }
        }
  
        /* average the pixel values
         */
  
        avgred= ((avgred + 8) / 9);
        avggreen= ((avggreen + 8) / 9);
        avgblue= ((avgblue + 8) / 9);
        pixval= (avgred << 16) | (avggreen << 8) | avgblue;
        valToMem(pixval, destptr, dest->pixlen);
        destptr += dest->pixlen;
      }
    }
  }
  if (src != isrc)	/* Free possible intermediate image */
    freeImage(src);
  return(dest);
}

Image *smooth(Image *isrc, int iterations, int verbose)
{ int a;
  Image *src=isrc,*tmp;

  if(GAMMA_NOT_EQUAL(src->gamma, 1.0))
    gammacorrect(src, 1.0, verbose);

  if (verbose) {
    printf("  Smoothing...");
    fflush(stdout);
  }

  for (a= 0; a < iterations; a++) {
    tmp= doSmooth(src);
    if(src != tmp && src != isrc)	/* free imtermediate image */
      freeImage(src);
    src= tmp;
  }

  if (verbose)
    printf("done\n");

  return(src);
}
