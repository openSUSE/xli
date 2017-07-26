/* faces.c:
 *
 * faces format image loader
 *
 * jim frost 07.06.89
 *
 * Copyright 1989 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */

#include "copyright.h"
#include "xli.h"
#include "imagetypes.h"
#include <string.h>

/* read a hex value and return its value
 */

static int nextInt(ZFILE *zf, unsigned int len)
{ int c;
  int value= 0;
  int count;

  len <<= 1;
  for (count= 0; count < len;) {
    c= zgetc(zf);
    if (c == EOF)
      return(-1);
    else {
      c= LEHexTable[c & 0xff];
      switch(c) {
      case HEXDELIM_IGNORE:	/* ignore */
	break;
      case HEXBAD:		/* bad */
      case HEXSTART_BAD:
      case HEXDELIM_BAD:
	return(-1);
      default:
	value= (value << 4) + c;
	count++;
      }
    }
  }
  return(value);
}

static boolean
isFaces(ZFILE *zf, char *name, char *fname, char *lname, unsigned int *wp, unsigned int *hp, unsigned int *dp)
{
  char          buf[BUFSIZ];
  unsigned int  w, h, d, iw, ih, id;

  w= h= d= 0;
  fname[0]= lname[0]= '\0';
  while (zgets((byte *)buf, BUFSIZ - 1, zf)) {
    if (! strcmp(buf, "\n"))
      break;
    if (!strncmp(buf, "FirstName:", 10))
      {
       strncpy(fname, buf + 11, BUFSIZ - 1);
       fname[BUFSIZ - 1] = '\0';
      }
    else if (!strncmp(buf, "LastName:", 9))
      {
       strncpy(lname, buf + 10, BUFSIZ - 1);
       lname[BUFSIZ - 1] = '\0';
      }
    else if (!strncmp(buf, "Image:", 6)) {
      if (sscanf(buf + 7, "%d%d%d", &iw, &ih, &id) != 3) {
	fprintf(stderr,"facesLoad: %s - Bad image\n", name);
	return(FALSE);
      }
    }
    else if (!strncmp(buf, "PicData:", 8)) {
      if (sscanf(buf + 9, "%d%d%d", &w, &h, &d) != 3) {
	fprintf(stderr,"facesLoad: %s - Bad image\n", name);
	return(FALSE);
      }
    }
  }
  if (!w || !h || !d)
    return FALSE;
  *wp = w;
  *hp = h;
  *dp = d;
  return TRUE;
}

Image *facesLoad(char *fullname, ImageOptions *image_ops, boolean verbose)
{
  ZFILE        *zf;
  char         *name = image_ops->name;
  Image        *image;
  char          fname[BUFSIZ];
  char          lname[BUFSIZ];
  unsigned int  w, h, d;
  unsigned int  x, y;
  int           value;
  unsigned int  linelen;
  byte         *lineptr, *dataptr;
  int           nn, nd;

  if (! (zf= zopen(fullname))) {
    perror("facesLoad");
    return(NULL);
  }

  {
    unsigned int  tw, th, td;
    if (!isFaces(zf, name, fname, lname, &tw, &th, &td)) {
      zclose(zf);
      return NULL;
    }
    w = tw;
    h = th;
    d = td;
  }

  if (verbose)
    printf("%s is a  %dx%d %d-bit grayscale Faces Project image\n",
	   name, w, h, d);

  initLEHexTable();

  znocache(zf);
  image= newRGBImage(w, h, d);

  if ((!*fname && !*lname) || ('\n' == *fname && '\n' == *lname)) {
    image->title = dupString(name);
  } else {
    if (*fname) {
      fname[strlen(fname) - 1] = ' ';
    }
    if (*lname) {
      lname[strlen(lname) - 1] = '\0';
    }
    strncat(fname, lname, sizeof(fname) - strlen(fname) - 1);
    fname[sizeof(fname) - 1] = '\0';
    image->title = dupString(fname);
  }

  /* image is greyscale; build RGB map accordingly
   */

  for (x= 0; x < image->rgb.size; x++)
    *(image->rgb.red + x)= *(image->rgb.green + x)= *(image->rgb.blue + x)=
      (65536 / image->rgb.size) * x;
  image->rgb.used= image->rgb.size;

  /* read in image data
   */

  linelen= w * image->pixlen;
  lineptr= image->data + (h * linelen);
  nd = 8 - d;
  w = w / (8 / d);  
  for (y= 0; y < h; y++) {
    lineptr -= linelen;
    dataptr= lineptr;
    for (x= 0; x < w; x++) {
      if ((value= nextInt(zf, image->pixlen)) < 0) {
	fprintf(stderr,"facesLoad: %s - Bad image data (returning partial image)\n", fullname);
	zclose(zf);
	return(image);
      }
      for (nn = 0; nn < 8; nn += d)
        *(dataptr++)= ((value>>(nd-nn)) & (0xff >> nd));
    }
  }
  read_trail_opt(image_ops,zf,image,verbose);
  zclose(zf);
  return(image);
}

int facesIdent(char *fullname, char *name)
{
  ZFILE        *zf;
  char          fname[BUFSIZ];
  char          lname[BUFSIZ];
  unsigned int  w, h, d;

  if (! (zf= zopen(fullname))) {
    perror("facesIdent");
    return(FALSE);
  }

  if (!isFaces(zf, name, fname, lname, &w, &h, &d)) {
    zclose(zf);
    return(FALSE);
  }
  printf("%s is a  %dx%d %d-bit grayscale Faces Project image\n",
	 name, w, h, d);
  zclose(zf);
  return(TRUE);
}
