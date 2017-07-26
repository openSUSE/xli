/* sunraster.c:
 *
 * sun rasterfile image type
 *
 * jim frost 09.27.89
 *
 * Copyright 1989, 1991 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */

#include "copyright.h"
#include "xli.h"
#include "imagetypes.h"
#include "sunraster.h"

/* SUPPRESS 558 */
/* SUPPRESS 560 */

static void babble(char *name, struct rheader *header)
{
  printf("%s is a", name);
  switch (memToVal(header->type, 4)) {
  case ROLD:
    printf("n old-style");
    break;
  case RSTANDARD:
    printf(" standard");
    break;
  case RRLENCODED:
    printf(" run-length encoded");
    break;
  case RRGB:
    printf(" RGB"); /* RGB format instead of BGR */
    break;
  case RTIFF:
    printf(" TIFF");
    break;
  case RIFF:
    printf(" RIFF");
    break;
  default:
    printf(" unknown-type");
  }
  printf(" %ldx%ld", memToVal(header->width, 4), memToVal(header->height, 4));

  switch (memToVal(header->depth, 4)) {
  case 1:
    printf(" monochrome");
    break;
  case 8:
    printf(" 8 plane %s",
	   memToVal(header->maplen, 4) > 0 ? "color" : "greyscale");
    break;
  case 24:
    printf(" 24 plane color");
    break;

  case 32:
    /* isn't it nice how the sunraster.h file doesn't bother to mention that
     * 32-bit depths are allowed?
     */
    printf(" 32 plane color");
    break;
  }
  printf(" Sun rasterfile\n");
}

int sunRasterIdent(char *fullname, char *name)
{ ZFILE          *zf;
  struct rheader  header;
  int             r;

  if (! (zf= zopen(fullname))) {
    perror("sunRasterIdent");
    return(0);
  }
  switch (zread(zf, (byte *)&header, sizeof(struct rheader))) {

  case sizeof(struct rheader):
    if (memToVal(header.magic, 4) != RMAGICNUMBER) {
      r= 0;
      break;
    }
    babble(name, &header);
    r= 1;
    break;

  case -1:
    perror("sunRasterIdent");
  default:
    r= 0;
    break;
  }
  zclose(zf);
  return(r);
}

/* read either rl-encoded or normal image data
 * Return TRUE if read was ok
 */

static boolean sunread(ZFILE *zf, byte *buf, unsigned int len, unsigned int enc, char *name)
                      
                       
                       
                         /* true if encoded file */
                        
{ static byte repchar, remaining= 0;

  /* rl-encoded read
   */

  if (enc) {
    while (len--)
      if (remaining) {
	remaining--;
	*(buf++)= repchar;
      }
      else {
	if (zread(zf, &repchar, 1) != 1) {
	  fprintf(stderr,"sunRasterLoad: %s - Bad read on encoded image data\n",name);
	  return FALSE;
	}
	if (repchar == RESC) {
	  if (zread(zf, &remaining, 1) != 1) {
	    fprintf(stderr,"sunRasterLoad: %s - Bad read on encoded image data\n",name);
	    return FALSE;
	  }
	  if (remaining == 0)
	    *(buf++)= RESC;
	  else {
	    if (zread(zf, &repchar, 1) != 1) {
	      fprintf(stderr,"sunRasterLoad: %s - Bad read on encoded image data\n",name);
	      return FALSE;
	    }
	    *(buf++)= repchar;
	  }
	}
	else
	  *(buf++)= repchar;
      }
  }

  /* normal read
   */

  else {
    if (zread(zf, buf, len) < len) {
      fprintf(stderr,"sunRasterLoad: %s - Bad read on standard image data\n",name);
      return FALSE;
    }
  }
  return TRUE;
}

Image *sunRasterLoad(char *fullname, ImageOptions *image_ops, boolean verbose)
{ ZFILE        *zf;
  char         *name = image_ops->name;
  struct rheader  header;
  unsigned int    mapsize;
  byte           *map;
  byte           *mapred, *mapgreen, *mapblue;
  unsigned int    depth;
  unsigned int    linelen;   /* length of raster line in bytes */
  unsigned int    fill;      /* # of fill bytes per raster line */
  unsigned int    enc;
  byte            fillchar;
  Image          *image;
  byte           *lineptr;
  unsigned int    x, y;

  if (! (zf= zopen(fullname))) {
    perror("sunRasterLoad");
    return(NULL);
  }
  switch (zread(zf, (byte *)&header, sizeof(struct rheader))) {

  case sizeof(struct rheader):
    if (memToVal(header.magic, 4) != RMAGICNUMBER) {
      zclose(zf);
      return(NULL);
    }
    if (verbose)
      babble(name, &header);
    break;

  case -1:
    perror("sunRasterLoad");
  default:
    zclose(zf);
    return(NULL);
  }

  znocache(zf); /* turn off caching; we don't need it anymore */

  /* get an image to put the data in
   */

  depth= memToVal(header.depth, 4);
  switch(depth) {
  case 1:
    image= newBitImage(memToVal(header.width, 4),
		       memToVal(header.height, 4));
    break;
  case 8:
    image= newRGBImage(memToVal(header.width, 4),
		       memToVal(header.height, 4),
		       memToVal(header.depth, 4));
    break;
  case 24:
  case 32:
    image= newTrueImage(memToVal(header.width, 4),
		       memToVal(header.height, 4));
    linelen= image->width * image->pixlen;
    break;
  default:
    fprintf(stderr,"sunRasterLoad: %s - Bad depth %d (only 1, 8, 24 are valid)\n",name,depth);
    zclose(zf);
    return NULL;
  }
  image->title= dupString(name);

  /*
   *  Handle color map...
   */
  mapsize= memToVal(header.maplen, 4);
  if (mapsize) {
    map= lmalloc(mapsize);
    if (zread(zf, map, mapsize) < mapsize) {
      fprintf(stderr,"sunRasterLoad: %s - Bad read on colormap\n",name);
      lfree(map);
      freeImage(image);
      zclose(zf);
      return NULL;
    }
    mapsize /= 3;
    if (depth > 8) {
      fprintf(stderr,"sunRasterLoad: %s - Warning, true color image colormap being ignored\n",name);
    } else {	/* can handle 8 bit indexed rgb */
      if (mapsize > image->rgb.size) {
        fprintf(stderr,"sunRasterLoad: %s - Warning, colormap is too big for depth\n",name);
        mapsize = image->rgb.size;	/* rgb.size is set from depth */
      }
      mapred= map;
      mapgreen= mapred + mapsize;
      mapblue= mapgreen + mapsize;
      for (y= 0; y < mapsize; y++) {
        *(image->rgb.red + y)= (*(mapred++) << 8);
        *(image->rgb.green + y)= (*(mapgreen++) << 8);
        *(image->rgb.blue + y)= (*(mapblue++) << 8);
      }
      image->rgb.used= mapsize;
    }
    lfree(map);
  }

  /*
   *  Handle 8-bit greyscale via a simple ramp function...
   */
  else if (depth == 8) {
    for (mapsize = 256, y= 0; y < mapsize; y++) {
      *(image->rgb.red + y)= (y << 8);
      *(image->rgb.green + y)= (y << 8);
      *(image->rgb.blue + y)= (y << 8);
    }
    image->rgb.used= mapsize;
  }
  /* 24-bit and 32-bit handle themselves.  currently we don't support
   * a colormap for them.
   */

  enc= (memToVal(header.type, 4) == RRLENCODED);
  lineptr= image->data;

  /* if it's a 32-bit image, we read the line and then strip off the
   * top byte of each pixel to get truecolor format
   */

  if (depth >= 24) {
    byte *buf, *bp;

    linelen= image->width * (depth == 24 ? 3 : 4);
    fill= (linelen % 2 ? 1 : 0);
    buf= lmalloc(linelen);
    for (y= 0; y < image->height; y++) {
      if (!sunread(zf, buf, linelen, enc, name))
      {
        lfree(buf);
        zclose(zf);
        return(image);
      }
      bp= buf;
      if (memToVal(header.type, 4) != RRGB) {
        if (depth == 24) {
          for (x= 0; x < image->width; x++) {
            *(lineptr++)= *(bp + 2); /* red */
            *(lineptr++)= *(bp + 1); /* green */
            *(lineptr++)= *bp;       /* blue */
            bp += 3;
          }
        }
        else {
          for (x= 0; x < image->width; x++) {
            *(lineptr++)= *(bp + 3); /* red */
            *(lineptr++)= *(bp + 2); /* green */
            *(lineptr++)= *(bp + 1); /* blue */
            bp += 4;
          }
        }
      }
      else {	/* RGB */
        if (depth == 24) {
          for (x= 0; x < image->width; x++) {
            *(lineptr++)= *bp;       /* red */
            *(lineptr++)= *(bp + 1); /* green */
            *(lineptr++)= *(bp + 2); /* blue */
            bp += 3;
          }
        }
        else {
          for (x= 0; x < image->width; x++) {
            *(lineptr++)= *bp;       /* red */
            *(lineptr++)= *(bp + 1); /* green */
            *(lineptr++)= *(bp + 2); /* blue */
            bp += 4;
          }
        }
      }
      if (fill)
	if (!sunread(zf, &fillchar, fill, enc, name))
        {
          lfree(buf);
          zclose(zf);
          return(image);
        }
    }
    lfree(buf);
  }
  else {
  if (depth == 1)
    linelen= (image->width / 8) + (image->width % 8 ? 1 : 0);
  else
    linelen= image->width * image->pixlen;

  fill= (linelen % 2 ? 1 : 0);
    for (y= 0; y < image->height; y++) {
      if (!sunread(zf, lineptr, linelen, enc, name))
      {
        zclose(zf);
        return(image);
      }
      lineptr += linelen;
      if (fill)
	if (!sunread(zf, &fillchar, fill, enc, name))
        {
          zclose(zf);
          return(image);
        }
    }
  }
  read_trail_opt(image_ops,zf,image,verbose);
  zclose(zf);
  return(image);
}
