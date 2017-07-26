/* clip.c:
 *
 * return a new image which is a clipped subsection of the old image
 *
 * jim frost 10.04.89
 *
 * Copyright 1989, 1991 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */

#include "copyright.h"
#include "xli.h"

#define ABS(x)   ((x) < 0 ? -(x) : (x))

Image *clip(Image *iimage, int clipx, int clipy,
	unsigned int clipw, unsigned int cliph, ImageOptions *imgopp)
{
  Image *simage = iimage, *dimage;
  int  dclipx, dclipy;
  unsigned int  dclipw, dcliph;
  unsigned int  x, y;
  unsigned int  slinelen, dlinelen;
  unsigned int  start, dstart;
  byte          startmask, smask;
  byte          dstartmask, dmask;
  byte         *sp, *sline, *dp, *dline;
  boolean       border_shows = FALSE;
  Pixel         border_pv = 0;

  if (globals.verbose) {
    printf("  Clipping image...");
    fflush(stdout);
  }

  /* sane-ify clip area with respect to dimage
   */

  dclipx = 0;
  dclipy = 0;
  dclipw = clipw;
  dcliph = cliph;

  if (clipx < 0) {
    dclipx = -clipx;
    clipx = 0;
    border_shows = TRUE;
  }
  if (clipy < 0) {
    dclipy = -clipy;
    clipy = 0;
    border_shows = TRUE;
  }

  if (clipx + dclipw > simage->width) {
    if (clipx > simage->width)
      dclipw = 0;
    else
      dclipw = simage->width - clipx;
    border_shows = TRUE;
  }
  if (dclipx + dclipw > clipw) {
    if (dclipx > clipw)
      dclipw = 0;
    else
      dclipw = clipw - dclipx;
  }

  if (clipy + dcliph > simage->height) {
    if (clipy > simage->height)
      dcliph = 0;
    else
      dcliph = simage->height - clipy;
    border_shows = TRUE;
  }
  if (dclipy + dcliph > cliph) {
    if (dclipy > cliph)
      dcliph = 0;
    else
      dcliph = cliph - dclipy;
  }

  if (globals.verbose) {
    if (border_shows)
      printf("(Adding border)");
    printf("...");
    fflush(stdout);
  }

  /* If the background is going to show after clipping
   * (ie. we are clipping the image to to make it larger
   * rather than smaller), then look up a suitable pixel
   * value to use as the background value. If we can't find
   * a close match, allocate a new color, or as a last
   * resort, convert the image to a true color image
   */
  if (border_shows) {
    Intensity red,green,blue;
    if (imgopp->border) {
      red = imgopp->bordercol.red;
      green = imgopp->bordercol.green;
      blue = imgopp->bordercol.blue;
    } else {
      red = green = blue = 65535;	/* default is white */
    }

    /* Gamma correct the background colour to suit image */
    red = (int)(0.5 + 65535 * pow( (double)red / 65535.0, 1.0/simage->gamma ));
    green = (int)(0.5 + 65535 * pow( (double)green / 65535.0, 1.0/simage->gamma ));
    blue = (int)(0.5 + 65535 * pow( (double)blue / 65535.0, 1.0/simage->gamma ));

    switch (simage->type) {
    case IBITMAP:
    case IRGB:

      for (border_pv= 0; border_pv < simage->rgb.used; border_pv++) {
	if (*(simage->rgb.red + border_pv) == red
	    && *(simage->rgb.green + border_pv) == green
	    && *(simage->rgb.blue + border_pv) == blue)
	  break;
      }

      if (border_pv >= simage->rgb.used) {
        /* Failed to find exact color */
	border_pv = simage->rgb.used;
	if (border_pv >=  simage->rgb.size) {	/* can't fit another */
	  compress_cmap(simage, FALSE);		/* try compressing image */
	  border_pv = simage->rgb.used;
        }
	if (border_pv <  simage->rgb.size) {	/* can fit another */
	  *(simage->rgb.red + border_pv) = red;
	  *(simage->rgb.green + border_pv) = green;
	  *(simage->rgb.blue + border_pv) = blue;
	  simage->rgb.used++;
	} else {	/* else need more color space */
	  Image *tmp;
	  if (simage->depth >= 16) {	/* don't create irgb with depth > 16 */
	    tmp = expandtotrue(simage);
	    border_pv =  RGB_TO_TRUE(red,green,blue);
	  } else {	/* add more depth (copes with bitmap) */
	    tmp = expandirgbdepth(simage,simage->depth+1);
	    *(tmp->rgb.red + border_pv) = red;
	    *(tmp->rgb.green + border_pv) = green;
	    *(tmp->rgb.blue + border_pv) = blue;
	    tmp->rgb.used++;
	  }
	  /* free intemediate image, but preserve input */
	  if (simage != tmp && simage != iimage)
	    freeImage(simage);
	  simage = tmp;
	}
      }
      break;
    case ITRUE:
      border_pv =  RGB_TO_TRUE(red,green,blue);
      break;
    default:
      printf("clip: Unsupported image type\n");
      exit(1);
    }
  }

  switch (simage->type) {
  case IBITMAP:

    /* this could be sped up; i don't care
     */

    dimage= newBitImage(clipw, cliph);
    for (x= 0; x < simage->rgb.used; x++) {
      *(dimage->rgb.red + x)= *(simage->rgb.red + x);
      *(dimage->rgb.green + x)= *(simage->rgb.green + x);
      *(dimage->rgb.blue + x)= *(simage->rgb.blue + x);
    }
    if (border_shows)
      fill(dimage, 0, 0, clipw, cliph, border_pv);
    slinelen= (simage->width / 8) + (simage->width % 8 ? 1 : 0);
    start= clipx / 8;
    startmask= 0x80 >> (clipx % 8);
    sline= simage->data + (slinelen * clipy);
    dlinelen= (clipw / 8) + (clipw % 8 ? 1 : 0);
    dstart= dclipx / 8;
    dstartmask= 0x80 >> (dclipx % 8);
    dline= dimage->data + (dlinelen * dclipy);
    for (y= 0; y < dcliph; y++) {
      sp= sline + start;
      dp= dline + dstart;
      smask= startmask;
      dmask= dstartmask;
      for (x= 0; x < dclipw; x++) {
	if (*sp & smask)
	  *dp |= dmask;
	else
	  *dp &= ~dmask;
	if (! (smask >>= 1)) {
	  smask= 0x80;
	  sp++;
	}
	if (! (dmask >>= 1)) {
	  dmask= 0x80;
	  dp++;
	}
      }
      sline += slinelen;
      dline += dlinelen;
    }
    break;

  case IRGB:
  case ITRUE:
    if (RGBP(simage)) {
      dimage= newRGBImage(clipw, cliph, simage->depth);
      for (x= 0; x < simage->rgb.used; x++) {
	*(dimage->rgb.red + x)= *(simage->rgb.red + x);
	*(dimage->rgb.green + x)= *(simage->rgb.green + x);
	*(dimage->rgb.blue + x)= *(simage->rgb.blue + x);
      }
      dimage->rgb.used= simage->rgb.used;
    }
    else
      dimage= newTrueImage(clipw, cliph);
    if (border_shows)
      fill(dimage, 0, 0, clipw, cliph, border_pv);
    slinelen= simage->width * simage->pixlen;
    start= clipx * simage->pixlen;
    sline= simage->data + (clipy * slinelen);
    dlinelen = simage->pixlen * clipw;
    dstart= dclipx * simage->pixlen;
    dline= dimage->data + (dclipy * dlinelen);
    for (y= 0; y < dcliph; y++) {
      sp= sline + start;
      dp= dline + dstart;
      bcopy(sp,dp,simage->pixlen * dclipw);
      sline += slinelen;
      dline += dlinelen;
    }
    break;
  default:
    printf("clip: Unsupported image type\n");
    exit(1);
  }
  dimage->title= dupString(simage->title);
  dimage->gamma= simage->gamma;
  if (simage != iimage)		/* free intemediate image, but preserver input */
    freeImage(simage);
  if (globals.verbose)
    printf("done\n");
  return(dimage);
}
