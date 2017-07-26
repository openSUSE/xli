/* bright.c
 *
 * miscellaneous colormap altering functions
 *
 * jim frost 10.10.89
 *
 * Copyright 1989, 1991 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */

#include "copyright.h"
#include "xli.h"

/* alter an image's brightness by a given percentage
 */

void brighten(Image *image, unsigned int percent, unsigned int verbose)
{ int          a;
  unsigned int newrgb;
  float        fperc;
  unsigned int size;
  byte        *destptr;

  if (BITMAPP(image))
    return;

  if(GAMMA_NOT_EQUAL(image->gamma, 1.0))
    gammacorrect(image, 1.0, verbose);

  if (verbose) {
    printf("  Brightening colormap by %d%%...", percent);
    fflush(stdout);
  }

  fperc= (float)percent / 100.0;

  switch (image->type) {
  case IRGB:
    for (a= 0; a < image->rgb.used; a++) {
      newrgb= *(image->rgb.red + a) * fperc;
      if (newrgb > 65535)
	newrgb= 65535;
      *(image->rgb.red + a)= newrgb;
      newrgb= *(image->rgb.green + a) * fperc;
      if (newrgb > 65535)
	newrgb= 65535;
      *(image->rgb.green + a)= newrgb;
      newrgb= *(image->rgb.blue + a) * fperc;
      if (newrgb > 65535)
	newrgb= 65535;
      *(image->rgb.blue + a)= newrgb;
    }
    break;

  case ITRUE:
    size= image->width * image->height * 3;
    destptr= image->data;
    for (a= 0; a < size; a++) {
      newrgb= *destptr * fperc;
      if (newrgb > 255)
	newrgb= 255;
      *(destptr++)= newrgb;
    }
    break;
  }
  if (verbose)
    printf("done\n");
}

void gammacorrect(Image *image, float target_gam, unsigned int verbose)
{ int          a;
  int gammamap[256];
  unsigned int size;
  byte        *destptr;

  if (BITMAPP(image)) {	/* bitmap gamma looks ok on any screen */
    image->gamma = target_gam;
    return;	
  }

  if (verbose) {
    printf("  Adjusting image gamma from %4.2f to %4.2f for image processing...",image->gamma,target_gam);
    fflush(stdout);
  }

  make_gamma(target_gam/image->gamma,gammamap);

  switch (image->type) {
  case IRGB:
    for (a= 0; a < image->rgb.used; a++) {
      *(image->rgb.red + a)= gammamap[(*(image->rgb.red + a))>>8]<<8;
      *(image->rgb.green + a)= gammamap[(*(image->rgb.green + a))>>8]<<8;
      *(image->rgb.blue + a)= gammamap[(*(image->rgb.blue + a))>>8]<<8;
    }
    break;

  case ITRUE:
    size= image->width * image->height * 3;
    destptr= image->data;
    for (a= 0; a < size; a++) {
      *destptr= gammamap[*destptr];
      destptr++;
    }
    break;
  }

  image->gamma = target_gam;
  if (verbose)
    printf("done\n");
}

/* 
 * Apply default gamma values to an image
 */
void defaultgamma(Image *image)
{
  if (BITMAPP(image)) {  /* We can't change the gamma of a bitmap */
    image->gamma = globals.display_gamma;
    if (globals.verbose) {
      printf("  Default gamma is arbitrary for bitmap\n");
      fflush(stdout);
    }
    return;
  }
  if (RGBP(image)) {  /* Assume 8 bit mapped images are gamma corrected */
    image->gamma = DEFAULT_IRGB_GAMMA;
    if (globals.verbose) {
      printf("  Default gamma for IRGB image is  %4.2f\n",image->gamma);
      fflush(stdout);
    }
    return;
  }
  if (TRUEP(image)) {  /* Assume a 24 bit image is linear */
    image->gamma = 1.0;
    if (globals.verbose) {
      printf("  Default gamma for ITRUE image is  %4.2f\n",image->gamma);
      fflush(stdout);
    }
  }
}

/* this initializes a lookup table for doing normalization
 */

static void setupNormalizationArray(unsigned int min, unsigned int max, byte *array, unsigned int verbose)
{ int a;
  unsigned int new;
  float factor;

  if (verbose) {
    printf("scaling %d:%d to 0:255...", min, max);
    fflush(stdout);
  }
  factor= 256.0 / (max - min);
  for (a= min; a <= max; a++) {
    new= (float)(a - min) * factor;
    array[a]= (new > 255 ? 255 : new);
  }
}

/* normalize an image.
 */

Image *normalize(Image *image, unsigned int verbose)
{ unsigned int  a, x, y;
  unsigned int  min, max;
  Pixel         pixval;
  Image        *newimage;
  byte         *srcptr, *destptr;
  byte          array[256];

  if (BITMAPP(image))
    return(image);

  if(GAMMA_NOT_EQUAL(image->gamma, 1.0))
    gammacorrect(image, 1.0, verbose);

  if (verbose) {
    printf("  Normalizing...");
    fflush(stdout);
  }
  switch (image->type) {
  case IRGB:
    min= 256;
    max = 0;
    for (a= 0; a < image->rgb.used; a++) {
      byte red, green, blue;

      red= image->rgb.red[a] >> 8;
      green= image->rgb.green[a] >> 8;
      blue= image->rgb.blue[a] >> 8;
      if (red < min)
	min= red;
      if (red > max)
	max= red;
      if (green < min)
	min= green;
      if (green > max)
	max= green;
      if (blue < min)
	min= blue;
      if (blue > max)
	max= blue;
    }
    setupNormalizationArray(min, max, array, verbose);

    newimage= newTrueImage(image->width, image->height);
    srcptr= image->data;
    destptr= newimage->data;
    for (y = 0; y < image->height; y++) {
      for (x = 0; x < image->width; x++) {
	pixval = memToVal(srcptr, image->pixlen);
	*destptr++ = array[image->rgb.red[pixval] >> 8];
	*destptr++ = array[image->rgb.green[pixval] >> 8];
	*destptr++ = array[image->rgb.blue[pixval] >> 8];
	srcptr += image->pixlen;
      }
    }
    newimage->title= dupString(image->title);
    newimage->gamma= image->gamma;
    break;

  case ITRUE:
    srcptr= image->data;
    min= 255;
    max= 0;
    for (y= 0; y < image->height; y++)
      for (x= 0; x < image->width; x++) {
	if (*srcptr < min)
	  min= *srcptr;
	if (*srcptr > max)
	  max= *srcptr;
	srcptr++;
	if (*srcptr < min)
	  min= *srcptr;
	if (*srcptr > max)
	  max= *srcptr;
	srcptr++;
	if (*srcptr < min)
	  min= *srcptr;
	if (*srcptr > max)
	  max= *srcptr;
	srcptr++;
      }
    setupNormalizationArray(min, max, array, verbose);

    srcptr= image->data;
    for (y= 0; y < image->height; y++)
      for (x= 0; x < image->width; x++) {
	*srcptr= array[*srcptr];
	srcptr++;
	*srcptr= array[*srcptr];
	srcptr++;
	*srcptr= array[*srcptr];
	srcptr++;
      }
    newimage= image;
    break;

  default:
    newimage= image;
    break;
  }

  if (verbose)
    printf("done\n");
  return(newimage);
}

/* convert to grayscale
 */

void gray(Image *image, int verbose)
{ int a;
  unsigned int size;
  Intensity intensity, red, green, blue;
  byte *destptr;

  if (BITMAPP(image))
    return;

  if(GAMMA_NOT_EQUAL(image->gamma, 1.0))
    gammacorrect(image, 1.0, verbose);

  if (verbose) {
    printf("  Converting image to grayscale...");
    fflush(stdout);
  }
  switch (image->type) {
  case IRGB:
    for (a= 0; a < image->rgb.used; a++) {
      intensity= colorIntensity(image->rgb.red[a],
				image->rgb.green[a],
				image->rgb.blue[a]);
      image->rgb.red[a]= intensity;
      image->rgb.green[a]= intensity;
      image->rgb.blue[a]= intensity;
    }
    break;

  case ITRUE:
    size= image->width * image->height;
    destptr= image->data;
    for (a= 0; a < size; a++) {
      red= *destptr << 8;
      green= *(destptr + 1) << 8;
      blue= *(destptr + 2) << 8;
      intensity= ((Intensity)colorIntensity(red, green, blue)) >> 8;
      *(destptr++)= intensity; /* red */
      *(destptr++)= intensity; /* green */
      *(destptr++)= intensity; /* blue */
    }
    break;
  }
  if (verbose)
    printf("done\n");
}
