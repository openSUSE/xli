/*
 * fbm.c:
 *
 * adapted from code by Michael Mauldin, (mlm) at Carnegie-Mellon
 * University, (fbm tools) and Kirk L. Johnson, (tuna@athena.mit.edu),
 * (gif.c).
 *
 * fbmin.c
 * Mark Majhor
 * August 1990
 *
 * routines for reading FBM files
 *
 * Copyright 1990 Mark Majhor (see the included file
 * "mrmcpyrght.h" for complete copyright information)
 */
# include "xli.h"
# include <ctype.h>
# include <string.h>
#define FBM_C
# include "fbm.h"
# include "imagetypes.h"

/****
 **
 ** local variables
 **
 ****/

static BYTE file_open = 0;	/* status flags */
static BYTE image_open = 0;

static ZFILE *ins;		/* input stream */
static FBMFILEHDR phdr;		/* header structure */

/****
 **
 ** global variables
 **
 ****/

static int  fbmin_img_width;           /* image width */
static int  fbmin_img_height;          /* image height */
static int  fbmin_img_depth;	       /* Depth (1 for B+W, 3 for RGB) */
static int  fbmin_img_bits;	       /* Bits per pixel */
static int  fbmin_img_rowlen;	       /* length of one row of data */
static int  fbmin_img_plnlen;	       /* length of one plane of data */
static int  fbmin_img_clrlen;	       /* length of the colormap */
static double  fbmin_img_aspect;       /* image aspect ratio */
static int  fbmin_img_physbits;	       /* physical bits per pixel */
static char *fbmin_img_title;	       /* name of image */
static char *fbmin_img_credit;	       /* credit for image */

static int fbmin_image_test(void);

static char *
get_err_string(int errno)
{
  int i;
  for (i=0;fbm_err_strings[i].err_no != 0;i++)
  {
    if (fbm_err_strings[i].err_no == errno)
		return fbm_err_strings[i].name;
  }
  return "";
}

/*
 * open FBM image in the input stream; returns FBMIN_SUCCESS if
 * successful. (might also return various FBMIN_ERR codes.)
 */
static int fbmin_open_image(ZFILE *s)
{
  char *hp;		/* header pointer */

  /* make sure there isn't already a file open */
  if (file_open)
    return(FBMIN_ERR_FAO);

  /* remember that we've got this file open */
  file_open = 1;
  ins = s;

  /* read in the fbm file header */
  hp = (char *) &phdr;
  if (zread(ins, (byte *)hp, sizeof(phdr)) != sizeof(phdr))
    return FBMIN_ERR_BAD_SIG;	/* can't be FBM file */

  if (strncmp(FBM_MAGIC, phdr.magic, sizeof(FBM_MAGIC)) != 0)
    return FBMIN_ERR_BAD_SIG;

  /* Now extract relevant features of FBM file header */
  fbmin_img_width    = atoi(phdr.cols);
  fbmin_img_height   = atoi(phdr.rows);
  fbmin_img_depth    = atoi(phdr.planes);
  fbmin_img_bits     = atoi(phdr.bits);
  fbmin_img_rowlen   = atoi(phdr.rowlen);
  fbmin_img_plnlen   = atoi(phdr.plnlen);
  fbmin_img_clrlen   = atoi(phdr.clrlen);
  fbmin_img_aspect   = atof(phdr.aspect);
  fbmin_img_physbits = atoi(phdr.physbits);
  fbmin_img_title    = phdr.title;
  fbmin_img_credit   = phdr.credits;

  if (fbmin_image_test() != FBMIN_SUCCESS)
    return FBMIN_ERR_BAD_SD;

  return FBMIN_SUCCESS;
}

/*
 * close an open FBM file
 */

static int fbmin_close_file(void)
{
  /* make sure there's a file open */
  if (!file_open)
    return FBMIN_ERR_NFO;

  /* mark file (and image) as closed */
  file_open  = 0;
  image_open = 0;

  /* done! */
  return FBMIN_SUCCESS;
}
    
static int fbmin_image_test(void)
{
  if (fbmin_img_width < 1 || fbmin_img_width > 32767) {
    fprintf (stderr, "Invalid width (%d) on input\n", fbmin_img_width);
    return FBMIN_ERR_BAD_SD;
  }

  if (fbmin_img_height < 1 || fbmin_img_height > 32767) {
    fprintf (stderr, "Invalid height (%d) on input\n", fbmin_img_height);
    return (0);
  }

  if (fbmin_img_depth != 1 && fbmin_img_depth != 3) {
    fprintf (stderr, "Invalid number of planes (%d) on input %s\n",
	     fbmin_img_depth, "(must be 1 or 3)");
    return FBMIN_ERR_BAD_SD;
  }

  if (fbmin_img_bits < 1 || fbmin_img_bits > 8) {
    fprintf (stderr, "Invalid number of bits (%d) on input %s\n",
	     fbmin_img_bits, "(must be [1..8])");
    return FBMIN_ERR_BAD_SD;
  }

  if (fbmin_img_physbits != 1 && fbmin_img_physbits != 8) {
    fprintf (stderr, "Invalid number of physbits (%d) on input %s\n",
	     fbmin_img_physbits, "(must be 1 or 8)");
    return FBMIN_ERR_BAD_SD;
  }

  if (fbmin_img_rowlen < 1 || fbmin_img_rowlen > 32767) {
    fprintf (stderr, "Invalid row length (%d) on input\n",
	     fbmin_img_rowlen);
    return FBMIN_ERR_BAD_SD;
  }

  if (fbmin_img_depth > 1 && fbmin_img_plnlen < 1) { 
    fprintf (stderr, "Invalid plane length (%d) on input\n",
	     fbmin_img_plnlen);
    return FBMIN_ERR_BAD_SD;
  }

  if (fbmin_img_plnlen < (fbmin_img_height * fbmin_img_rowlen)) {
    fprintf (stderr, "Invalid plane length (%d) < (%d) * (%d) on input\n",
	     fbmin_img_plnlen,fbmin_img_height,fbmin_img_rowlen);
    return FBMIN_ERR_BAD_SD;
  }

  if (fbmin_img_aspect < 0.01 || fbmin_img_aspect > 100.0) {
    fprintf (stderr, "Invalid aspect ratio %g on input\n",
	     fbmin_img_aspect);
    return FBMIN_ERR_BAD_SD;
  }
    return FBMIN_SUCCESS;
}

/*
 * these are the routines added for interfacing to xli
 */

/*
 * tell someone what the image we're loading is.  this could be a little more
 * descriptive but I don't care
 */

static void tellAboutImage(char *name)
{
  printf("%s is a %dx%d FBM image, depth %d with %d colors\n", name,
    fbmin_img_width, fbmin_img_height, fbmin_img_bits * fbmin_img_depth,
    fbmin_img_clrlen / 3);
}

Image *fbmLoad(char *fullname, ImageOptions *image_ops, boolean verbose)
{
  ZFILE        *zf;
  char         *name = image_ops->name;
  Image *image;
  register int    x, y, j, k, width, rowpad, plnpad;
  unsigned char *pixptr, *cm;
  unsigned char *r, *g, *b;
  int retv;

  CURRFUNC("fbmLoad");
  if (! (zf= zopen(fullname)))
    return(NULL);
  if ((retv = fbmin_open_image(zf)) != FBMIN_SUCCESS) {  /* read image header */
    fbmin_close_file();
    zclose(zf);
    if(retv != FBMIN_ERR_BAD_SIG)
      fprintf (stderr, "fbmLoad: %s - aborting, '%s'\n",name, get_err_string(retv));
    return(NULL);
  }
  if (verbose)
    tellAboutImage(name);
  znocache(zf);

  if(fbmin_img_depth==1)
    if(fbmin_img_bits==1)
      image= newBitImage(fbmin_img_width, fbmin_img_height);
    else
      image = newRGBImage(fbmin_img_width, fbmin_img_height, fbmin_img_bits);
  else	/* must be 3 */
    image = newTrueImage(fbmin_img_width, fbmin_img_height);
  image->title= dupString(name);

  /* if image has a local colormap, override global colormap
   */
  if (fbmin_img_depth==1 && fbmin_img_clrlen > 0) {
    cm = (unsigned char *) lmalloc(fbmin_img_clrlen);

    if (zread(ins, cm, fbmin_img_clrlen) != fbmin_img_clrlen) {
      fprintf (stderr, "fbmLoad: %s - can't read colormap (%d bytes)\n", name, fbmin_img_clrlen);
      fbmin_close_file();
      lfree(cm);
      zclose(zf);
      freeImage(image);
      return(NULL);
    }
    /*
     * fbm color map is organized as
     * buf[3][16]
     */
    y = fbmin_img_clrlen / 3;
    r = &cm[0], g = &cm[y], b = &cm[2 * y];
    for (x = 0; x < y; x++, r++, g++, b++) {
      image->rgb.red[x]   = *r << 8;
      image->rgb.green[x] = *g << 8;
      image->rgb.blue[x]  = *b << 8;
    }
    image->rgb.used = y;

  } else
    cm = NULL;

  width = fbmin_img_width;
  rowpad = fbmin_img_rowlen - width;
  plnpad = fbmin_img_plnlen - (fbmin_img_height * fbmin_img_rowlen);

  if(fbmin_img_depth == 1) {
    if(fbmin_img_bits==1) {
      int xx,bb;
      byte *rb;
      rb = (byte *) lmalloc(width > rowpad ? width : rowpad);
      pixptr = image->data;
      for (j = 0; j < fbmin_img_height; j++, pixptr += ((width+7)/8)) {
        if (zread(ins, rb, width) != width) {
          fprintf(stderr, "fbmLoad: %s - Short read within image data\n", name);
	  if (cm != NULL)
	    lfree(cm);
          lfree(rb);
	  fbmin_close_file();
	  zclose(zf);
	  return(image);
        }
        for (xx = 0,bb=0; xx < width; xx++) {
		  bb = (bb << 1) | (rb[xx] ? 0 : 1);
		  if ((xx & 7) == 7) {
            pixptr[xx/8] = bb;
            bb = 0;
		  }
        }
        if (zread(ins, rb, rowpad) != rowpad) {
          fprintf(stderr, "fbmLoad: %s - Short read within row padding\n", name);
	  if (cm != NULL)
	    lfree(cm);
	  lfree(rb);
	  fbmin_close_file();
	  zclose(zf);
	  return(image);
        }
      }
      lfree(rb);
    }
    else {
      byte *rb;
      rb = (byte *) lmalloc(rowpad);
      pixptr = image->data;
      for (j = 0; j < fbmin_img_height; j++, pixptr += width) {
        if (zread(ins, pixptr, width) != width) {
          fprintf(stderr, "fbmLoad: %s - Short read within image data\n", name);
	  if (cm != NULL)
	    lfree(cm);
	  lfree(rb);
	  fbmin_close_file();
	  zclose(zf);
	  return(image);
        }
        if (zread(ins, rb, rowpad) != rowpad) {
          fprintf(stderr,"fbmLoad: %s - Short read within row padding\n", name);
	  if (cm != NULL)
	    lfree(cm);
	  lfree(rb);
	  fbmin_close_file();
	  zclose(zf);
	  return(image);
        }
      }
      lfree(rb);
    }
  }
  else {    /* depth == 3 case */
    int xx;
    byte *rb;
    rb = (byte *) lmalloc(width > rowpad ? (width > plnpad ? width : plnpad)
                                          :(rowpad > plnpad ? rowpad : plnpad));
    for (k = 0; k < fbmin_img_depth; k++) {
      pixptr = image->data + k;
      for (j = 0; j < fbmin_img_height; j++) {
        if (zread(ins, rb, width) != width) {
          fprintf(stderr, "fbmLoad: %s - Short read within image data\n", name);
	  if (cm != NULL)
	    lfree(cm);
	  lfree(rb);
	  fbmin_close_file();
	  zclose(zf);
	  return(image);
        }
        for (xx = 0; xx < width; xx++,pixptr+=3) {
          *pixptr = rb[xx];
        }
        if (zread(ins, rb, rowpad) != rowpad) {
          fprintf(stderr,"fbmLoad: %s - Short read within row padding\n", name);
	  if (cm != NULL)
	    lfree(cm);
	  lfree(rb);
	  fbmin_close_file();
	  zclose(zf);
	  return(image);
        }
      }
      if (zread(ins, rb, plnpad) != plnpad) {
        fprintf(stderr,"fbmLoad: %s - Short read within plane padding\n", name);
	if (cm != NULL)
	  lfree(cm);
	lfree(rb);
	fbmin_close_file();
	zclose(zf);
	return(image);
      }
    }
  lfree(rb);
  }

  if (cm != NULL)
    lfree(cm);
  fbmin_close_file();
  read_trail_opt(image_ops,zf,image,verbose);
  zclose(zf);
  return(image);
}

int fbmIdent(char *fullname, char *name)
{
  ZFILE        *zf;
  unsigned int  ret;
  int retv;

  if (! (zf= zopen(fullname)))
    return(0);
  if ((retv = fbmin_open_image(zf)) == FBMIN_SUCCESS) {
    tellAboutImage(name);
    ret = 1;
  } else {
    if (retv != FBMIN_ERR_BAD_SIG) {
      fprintf (stderr, "FBM file error\n");
      ret = 1;		/* file was identified as FBM */
    } else {
      ret = 0;
    }
  }
  fbmin_close_file();
  zclose(zf);
  return(ret);
}
