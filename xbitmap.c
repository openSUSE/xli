/* xbitmap.c:
 *
 * at one time this was XRdBitF.c.  it bears very little resemblence to it
 * now.  that was ugly code.  this is cleaner, faster, and more reliable
 * in most cases.
 *
 * jim frost 10.06.89
 *
 * Copyright, 1987, Massachusetts Institute of Technology
 *
 * Copyright 1989 Jim Frost.  See included file "copyright.h" for complete
 * copyright information.
 */

#include "mit.cpyrght"
#include "copyright.h"
#include "xli.h"
#include "imagetypes.h"
#include <ctype.h>

#define MAX_SIZE 255

/* read a hex value and return its value
 */

static int nextInt(ZFILE *zf)
{ int c;
  int value= 0;
  int shift= 0;
    
  for (;;) {
    c= zgetc(zf);
    if (c == EOF)
      return(-1);
    else {
      c= BEHexTable[c & 0xff];
      switch(c) {
      case HEXSTART_BAD:	/* start */
	shift= 0; /* reset shift counter */
	break;
      case HEXDELIM_BAD:	/* delim */
      case HEXDELIM_IGNORE:
	if (shift)
	  return(value);
	break;
      case HEXBAD:		/* bad */
	return(-1);
      default:
	value += (c << shift);
	shift += 4;
      }
    }
  }
}

/*
 * Return TRUE if the given file is an xbitmap file
 */

static boolean isXbitmap(ZFILE *zf, unsigned int *w, unsigned int *h, int *v10p, char **title)
{
  char          line[MAX_SIZE];
  char          name_and_type[MAX_SIZE];
  char         *type;
  int           value;
  int           maxlines=8;	/* 8 lines to find #define etc. */

  *title = NULL;
  *w = 0;
  *h = 0;

  /* get width/height values */

  while (zgets((byte *)line, MAX_SIZE, zf) && maxlines-- > 0) {
    if (strlen(line) == MAX_SIZE-1) {
      return FALSE;
    }

    /* width/height/hot_x/hot_y scanning
     */

    if (sscanf(line,"#define %s %d", name_and_type, &value) == 2) {
      if (!(type = rindex(name_and_type, '_')))
	type = name_and_type;
      else
	type++;

      if (!strcmp("width", type))
	*w = (unsigned int)value;
      if (!strcmp("height", type))
	*h = (unsigned int)value;
    }

    /* if start of data, determine if it's X10 or X11 data and break
     */

    if (sscanf(line, "static short %s = {", name_and_type) == 1) {
      *v10p = 1;
      break;
    }
    if ((sscanf(line,"static unsigned char %s = {", name_and_type) == 1) ||
	(sscanf(line, "static char %s = {", name_and_type) == 1)) {
      *v10p = 0;
      break;
    }
  }

  if (!*w || !*h) {
    return FALSE;
  }

  /* get title of bitmap if any
   */

  if ((type = rindex(name_and_type, '_')) && !strcmp("bits[]", type + 1)) {
    *type= '\0';
    *title= dupString(name_and_type);
  }
  return TRUE;
}

Image *xbitmapLoad(char *fullname, ImageOptions *image_ops, boolean verbose)
{ ZFILE        *zf;
  char         *name = image_ops->name;
  Image        *image;
  char         *title;
  int           value;
  int           v10p;
  unsigned int  linelen, dlinelen;
  unsigned int  x, y;
  unsigned int  w, h;
  byte         *dataptr;

  if (! (zf= zopen(fullname))) {
    perror("xbitmapLoad");
    return(NULL);
  }

  /* See if it's an x bitmap */
  {
    unsigned int  tw, th;
    int           tv10p;
    if (!isXbitmap(zf, &tw, &th, &tv10p, &title))
    {
      zclose(zf);
      return NULL;
    }
    w = tw;
    h = th;
    v10p = tv10p;
  }

  image= newBitImage(w, h);

  if (title != NULL)
    image->title= title;
  else
    image->title= dupString(name);

  if (verbose) {
    printf("%s is a %dx%d X%s bitmap file titled '%s'\n",
           name, image->width, image->height, v10p ? "10" : "11", image->title);
  }
    
  /* read bitmap data
   */

  initBEHexTable();

  znocache(zf);
  linelen= (w / 8) + (w % 8 ? 1 : 0); /* internal line length */
  if (v10p) {
    dlinelen= (w / 8) + (w % 16 ? 2 : 0);
    dataptr= image->data;
    for (y= 0; y < h; y++) {
      for (x= 0; x < dlinelen; x++) {
	if ((value= nextInt(zf)) < 0) {
          fprintf(stderr,"xbitmapLoad: %s - short read on X bitmap file\n", name);
	  zclose(zf);
	  return(image);
	}
	*(dataptr++)= value >> 8;
	if (++x < linelen)
	  *(dataptr++)= value & 0xff;
      }
    }
  }
  else {
    dataptr= image->data;
    for (y= 0; y < h; y++)
      for (x= 0; x < linelen; x++) {
	if ((value= nextInt(zf)) < 0)
        {
          fprintf(stderr,"xbitmapLoad: %s - short read on X bitmap file\n", name);
          zclose(zf);
          return(image);
        }
	*(dataptr++)= value;
      }
  }

  read_trail_opt(image_ops,zf,image,verbose);
  zclose(zf);
  return(image);
}

int xbitmapIdent(char *fullname, char *name)
{ ZFILE        *zf;
  char         *title;
  unsigned int  w, h;
  int           v10p;

  if (! (zf= zopen(fullname))) {
    perror("xbitmapIdent");
    return(0);
  }

  /* See if it's an x bitmap */
  if (!isXbitmap(zf, &w, &h, &v10p, &title))
  {
    zclose(zf);
    return 0;
  }

  if (title == NULL)
    title= dupString(name);
  
  printf("%s is a %dx%d X%s bitmap file titled '%s'\n",
         name, w, h, v10p ? "10" : "11",title);

  lfree(title);
  zclose(zf);
  return 1;
}

