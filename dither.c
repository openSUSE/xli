/* #ident	"@(#)x11:contrib/clients/xloadimage/dither.c 6.13 94/07/29 Labtam" */
/* dither.c
 *
 * completely reworked dithering module for xli
 * uses error-diffusion dithering (floyd-steinberg) instead
 * of simple 4x4 ordered-dither that was previously used
 *
 * the previous version of this code was written by steve losen
 * (scl@virginia.edu)
 * 
 * jim frost    07.10.89
 * Steve Losen  11.17.89
 * kirk johnson 06.04.90
 * Graeme Gill  16/10/92 - remove tone_scale_adjust() stuff - 
 *			   gamma adjustment is better. Clean up values so
 * 			   that dithering mono images doesn't change them!
 *
 * Copyright 1990 Kirk L. Johnson (see the included file
 * "kljcpyrght.h" for complete copyright information)
 *
 * Copyright 1989, 1990 Jim Frost and Steve Losen.
 * See included file "copyright.h" for complete copyright information.
 */

#include "copyright.h"
#include "kljcpyrght.h"
#include "xli.h"

#define MaxGrey       65280         /* limits on the grey levels used */
#define Threshold     (MaxGrey/2)	/* in the dithering process */
#define MinGrey           0

static void         LeftToRight(int *curr, int *next, int width);
static void         RightToLeft(int *curr, int *next, int width);


/*
 * simple floyd-steinberg dither with serpentine raster processing
 */

Image *dither(Image *cimage, unsigned int verbose)
{
  Image          *image;	/* destination image */
  int            *grey;		/* grey map for source image */
  unsigned int    spl;		/* source pixel length in bytes */
  unsigned int    dll;		/* destination line length in bytes */
  unsigned char  *src;		/* source data */
  unsigned char  *dst;		/* destination data */
  int            *curr;		/* current line buffer */
  int            *next;		/* next line buffer */
  int            *swap;		/* for swapping line buffers */
  Pixel           color;	/* pixel color */
  int             level;	/* grey level */
  unsigned int    i, j;		/* loop counters */

  CURRFUNC("dither");
  /*
   * check the source image
   */
  if (BITMAPP(cimage))
    return(NULL);

  if(GAMMA_NOT_EQUAL(cimage->gamma, 1.0))
    gammacorrect(cimage, 1.0, verbose);

  /*
   * allocate destination image
   */
  if (verbose)
  {
    printf("  Dithering image...");
    fflush(stdout);
  }
  image = newBitImage(cimage->width, cimage->height);
  if (cimage->title)
  {
    image->title = (char *)lmalloc(strlen(cimage->title) + 12);
    sprintf(image->title, "%s (dithered)", cimage->title);
  }
  image->gamma= cimage->gamma;

  /*
   * if the number of entries in the colormap isn't too large, compute
   * the grey level for each entry and store it in grey[]. else the
   * grey levels will be computed on the fly.
   */
  if (RGBP(cimage) && (cimage->depth <= 16))
  {
    grey = (int *)lmalloc(sizeof(int) * cimage->rgb.used);
    for (i=0; i<cimage->rgb.used; i++)
      grey[i]=
	((int)colorIntensity(cimage->rgb.red[i],
			cimage->rgb.green[i],
			cimage->rgb.blue[i]));

  }
  else
  {
    grey = NULL;
  }

  /*
   * dither setup
   */
  spl = cimage->pixlen;
  dll = (image->width / 8) + (image->width % 8 ? 1 : 0);
  src = cimage->data;
  dst = image->data;

  curr  = (int *)lmalloc(sizeof(int) * (cimage->width + 2));
  next  = (int *)lmalloc(sizeof(int) * (cimage->width + 2));
  curr += 1;
  next += 1;
  bzero ((char *)curr, cimage->width * sizeof(*curr));
  bzero ((char *)next, cimage->width * sizeof(*next));

  /*
   * primary dither loop
   */
  for (i=0; i<cimage->height; i++)
  {
    /* copy the row into the current line */
    for (j=0; j<cimage->width; j++)
    {
      color = memToVal(src, spl);
      src  += spl;
      
      if (RGBP(cimage)) {
	if (grey == NULL)
	  level = (int)colorIntensity(cimage->rgb.red[color],
					     cimage->rgb.green[color],
					     cimage->rgb.blue[color]);
	else
	  level = (int)grey[color];
      }
      else {
	level = (int)colorIntensity((TRUE_RED(color) << 8),
					   (TRUE_GREEN(color) << 8),
					   (TRUE_BLUE(color) << 8));
      }
      curr[j] += level;
    }

    /* dither the current line */
    if (i & 0x01)
      RightToLeft(curr, next, cimage->width);
    else
      LeftToRight(curr, next, cimage->width);

    /* copy the dithered line to the destination image */
    for (j=0; j<cimage->width; j++)
      if (curr[j] < Threshold)
	dst[j / 8] |= 1 << (7 - (j & 7));
    dst += dll;
    
    /* circulate the line buffers */
    swap = curr;
    curr = next;
    next = swap;
    bzero ((char *)next, cimage->width * sizeof(*next));
  }

  /*
   * clean up
   */
  if (grey != NULL)
    lfree((byte *)grey);
  lfree((byte *)(curr-1));
  lfree((byte *)(next-1));
  if (verbose)
    printf("done\n");
  return(image);
}


/*
 * dither a line from left to right
 */
static void LeftToRight(int *curr, int *next, int width)
{
  int idx;
  int error;
  int output;

  if (0 < width)
  {
    output       = (curr[0] > Threshold) ? MaxGrey : MinGrey;
    error        = curr[0] - output;
    curr[0]    = output;
    next[0]   += error * 5 / 16;
    if (width > 1)
    {
      next[1] += error * 1 / 16;
      curr[1] += error * 7 / 16;
    }
  }

  for (idx=1; idx<(width-1); idx++)
  {
    output       = (curr[idx] > Threshold) ? MaxGrey : MinGrey;
    error        = curr[idx] - output;
    curr[idx]    = output;
    next[idx-1] += error * 3 / 16;
    next[idx]   += error * 5 / 16;
    next[idx+1] += error * 1 / 16;
    curr[idx+1] += error * 7 / 16;
  }

  idx = width -1;
  if (idx > 0)
  {
    output       = (curr[idx] > Threshold) ? MaxGrey : MinGrey;
    error        = curr[idx] - output;
    curr[idx]    = output;
    next[idx-1] += error * 3 / 16;
    next[idx]   += error * 5 / 16;
  }
}


/*
 * dither a line from right to left
 */
static void RightToLeft(int *curr, int *next, int width)
{
  int idx;
  int error;
  int output;

  idx = width -1;
  if (idx >= 0)
  {
    output       = (curr[idx] > Threshold) ? MaxGrey : MinGrey;
    error        = curr[idx] - output;
    curr[idx]    = output;
    next[idx]   += error * 5 / 16;
    if (idx > 0)
    {
      next[idx-1] += error * 1 / 16;
      curr[idx-1] += error * 7 / 16;
    }
  }

  for (idx=(width-2); idx>=1; idx--)
  {
    output       = (curr[idx] > Threshold) ? MaxGrey : MinGrey;
    error        = curr[idx] - output;
    curr[idx]    = output;
    next[idx+1] += error * 3 / 16;
    next[idx]   += error * 5 / 16;
    next[idx-1] += error * 1 / 16;
    curr[idx-1] += error * 7 / 16;
  }

  if (0 < width)
  {
    output       = (curr[0] > Threshold) ? MaxGrey : MinGrey;
    error        = curr[0] - output;
    curr[0]    = output;
    next[0]   += error * 5 / 16;
  }
}
