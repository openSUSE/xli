/* #ident	"@(#)x11:contrib/clients/xloadimage/send.c 6.21 94/07/29 Labtam" */
/* send.c:
 *
 * send an Image to an X pixmap
 *
 * jim frost 10.02.89
 *
 * Copyright 1989, 1990, 1991 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */

#include "copyright.h"
#include "xli.h"
#include <assert.h>

/* extra colors to try allocating in private color maps to minimise flashing */
#define NOFLASH_COLORS 256
/* number of colors to allow in the default colormap */
#define DEFAULT_COLORS 16

static int GotError;

static int pixmapErrorTrap(Display *disp, XErrorEvent *pErrorEvent)
{
#define MAXERRORLEN 100
    char buf[MAXERRORLEN+1];
    GotError = 1;
    XGetErrorText(disp, pErrorEvent->error_code, buf, MAXERRORLEN);
    printf("serial #%ld (request code %d) Got Error %s\n",
	pErrorEvent->serial,
	pErrorEvent->request_code,
	buf);
    return(0);
}

Pixmap ximageToPixmap(Display *disp, Window parent, XImageInfo *xii)
{
  int         (*old_handler)();
  Pixmap        pixmap;

  GotError = 0;
  old_handler = XSetErrorHandler(pixmapErrorTrap);
  XSync(disp, False);
  pixmap= XCreatePixmap(disp, parent, xii->ximage->width, xii->ximage->height,
    xii->depth);
  (void)XSetErrorHandler(old_handler);
  if (GotError)
    return(None);
  xii->drawable= pixmap;
  sendXImage(xii, 0, 0, 0, 0, xii->ximage->width, xii->ximage->height);
  return(pixmap);
}

/* find the best pixmap depth supported by the server for a particular
 * visual and return that depth.
 *
 * this is complicated by R3's lack of XListPixmapFormats so we fake it
 * by looking at the structure ourselves.
 */

static unsigned int bitsPerPixelAtDepth(Display *disp, int scrn, unsigned int depth)
{
#if defined(XlibSpecificationRelease) && (XlibSpecificationRelease >= 4)
  /* the way things are */
  XPixmapFormatValues *xf;
  int nxf, a;

  xf = XListPixmapFormats(disp, &nxf);
  for (a = 0; a < nxf; a++)
    if (xf[a].depth == depth)
      {
      int bpp;
      bpp = xf[a].bits_per_pixel;
      XFree(xf);
      return (unsigned int) bpp;
      }
  XFree(xf);
#else /* the way things were (X11R3) */
  unsigned int a;

  for (a= 0; a < disp->nformats; a++)
    if (disp->pixmap_format[a].depth == depth)
      return(disp->pixmap_format[a].bits_per_pixel);
#endif

  /* this should never happen; if it does, we're in trouble
   */

  fprintf(stderr, "bitsPerPixelAtDepth: Can't find pixmap depth info!\n");
  exit(1);
}


static int shmErrorTrap(Display *d, XErrorEvent *ev)
{
	GotError = 1;
	if (ev->error_code != BadAccess)
		pixmapErrorTrap(d, ev);

	return 0;
}


static void createImage(XImageInfo *xii, Image *image, Visual *visual,
	int depth, int format)
{
	if (XShmQueryExtension(xii->disp)) {
		xii->ximage = XShmCreateImage(xii->disp, visual, depth, format,
			   NULL, &xii->shm, image->width, image->height);
		xii->shm.shmid = shmget(IPC_PRIVATE,
			xii->ximage->bytes_per_line * image->height,
			IPC_CREAT | 0777);
		if (xii->shm.shmid >= 0) {
			int ret;
			XErrorHandler old_handler;

			xii->ximage->data = xii->shm.shmaddr =
				shmat(xii->shm.shmid, 0, 0);
			xii->shm.readOnly = False;
			XSync(xii->disp, False);
			GotError = 0;
			old_handler = XSetErrorHandler(shmErrorTrap);
			ret = XShmAttach(xii->disp, &xii->shm);
			XSync(xii->disp, False);
			(void) XSetErrorHandler(old_handler);
			if (ret && !GotError) {
				shmctl(xii->shm.shmid, IPC_RMID, 0);
				return;
			}
			shmdt(xii->shm.shmaddr);
			shmctl(xii->shm.shmid, IPC_RMID, 0);
			xii->shm.shmid = -1;
		}
		XDestroyImage(xii->ximage);
	}

	xii->ximage = XCreateImage(xii->disp, visual, depth, format,
		0, NULL, image->width, image->height, 8, 0);
	xii->ximage->data =
		lmalloc(image->height * xii->ximage->bytes_per_line);
}


XImageInfo *imageToXImage(Display *disp, int scrn, Visual *visual,
	unsigned int ddepth, Image *image, unsigned int private_cmap,
	unsigned int fit, ImageOptions *options)
{
  float        display_gamma = globals.display_gamma;
  unsigned int  verbose = globals.verbose;
  Pixel        *redvalue, *greenvalue, *bluevalue;
  unsigned int  a, b, c=0, newmap, x, y, dpixlen, dbits;
  XColor        xcolor;
  XImageInfo   *xii;
  Image        *orig_image;
  boolean	dogamma=FALSE;
  int           gammamap[256];

  CURRFUNC("imageToXImage");

  /* set up to adjust the image gamma for the display gamma we've got */
  if(GAMMA_NOT_EQUAL(display_gamma,image->gamma) && !(BITMAPP(image))) {
    make_gamma(display_gamma/image->gamma,gammamap);
    dogamma = TRUE;
  }

  xcolor.flags= DoRed | DoGreen | DoBlue;
  redvalue= greenvalue= bluevalue= NULL;
  orig_image= image;
  xii= (XImageInfo *)lmalloc(sizeof(XImageInfo));
  xii->disp= disp;
  xii->scrn= scrn;
  xii->depth= 0;
  xii->drawable= None;
  xii->index= NULL;
  xii->rootimage= FALSE;	/* assume not */
  xii->foreground= xii->background= 0;
  xii->gc= NULL;
  xii->ximage= NULL;
  xii->shm.shmid = -1;

  /* process image based on type of visual we're sending to */

  switch (image->type) {
  case ITRUE:
    switch (visual->class) {
    case TrueColor:
    case DirectColor:
      /* goody goody */
      break;
    default:
      if (visual->map_entries > 2) {
	Image *dimage;
	int ncols = visual->map_entries;
	if(ncols >= 256)
          ncols -= DEFAULT_COLORS;	/* allow for some default colors */
	dimage = reduce(image, ncols, options->colordither, display_gamma, verbose);
        if(dimage != image && orig_image != image)
          freeImage(image);
        image = dimage;
        if(GAMMA_NOT_EQUAL(display_gamma,image->gamma)) {
          make_gamma(display_gamma/image->gamma,gammamap);
          dogamma = TRUE;
        }
      }
      else {	/* it must be monochrome */
        Image *dimage;
        dimage= dither(image, verbose);
        if(dimage != image && orig_image != image)
          freeImage(image);
        image = dimage;
      }
    }
    break;

  case IRGB:
    switch(visual->class) {
    case TrueColor:
    case DirectColor:
      /* no problem, we handle this just fine */
      break;
    default:
      if (visual->map_entries <= 2) {	/* monochrome */
        Image *dimage;
	dimage= dither(image, verbose);
        if(dimage != image && orig_image != image)
          freeImage(image);
        image = dimage;
      }
      break;
    }

  case IBITMAP:
    /* no processing ever needs to be done for bitmaps */
    break;
  }

  /* do color allocation
   */

  switch (visual->class) {
  case TrueColor:
  case DirectColor:
    { Pixel pixval;
      unsigned int redcolors, greencolors, bluecolors;
      unsigned int redstep, greenstep, bluestep;
      unsigned int redbottom, greenbottom, bluebottom;
      unsigned int redtop, greentop, bluetop;

      redvalue= (Pixel *)lmalloc(sizeof(Pixel) * 256);
      greenvalue= (Pixel *)lmalloc(sizeof(Pixel) * 256);
      bluevalue= (Pixel *)lmalloc(sizeof(Pixel) * 256);

      if (visual == DefaultVisual(disp, scrn))
	xii->cmap= DefaultColormap(disp, scrn);
      else
	xii->cmap= XCreateColormap(disp, RootWindow(disp, scrn),
					  visual, AllocNone);

      retry_direct: /* tag we hit if a DirectColor allocation fails on
		     * default colormap */

      /* calculate number of distinct colors in each band
       */

      redcolors= greencolors= bluecolors= 1;
      for (pixval= 1; pixval; pixval <<= 1) {
	if (pixval & visual->red_mask)
	  redcolors <<= 1;
	if (pixval & visual->green_mask)
	  greencolors <<= 1;
	if (pixval & visual->blue_mask)
	  bluecolors <<= 1;
      }
      
      /* sanity check
       */

      if ((redcolors > visual->map_entries) ||
	  (greencolors > visual->map_entries) ||
	  (bluecolors > visual->map_entries)) {
	fprintf(stderr, "Warning: inconsistency in color information (this may be ugly)\n");
      }

      redstep = 256 / redcolors;
      greenstep = 256 / greencolors;
      bluestep = 256 / bluecolors;
      redbottom = greenbottom = bluebottom = 0;
      redtop = greentop = bluetop = 0;
      for (a = 0; a < visual->map_entries; a++) {
	if (redbottom < 256)
	  redtop= redbottom + redstep;
	if (greenbottom < 256)
	  greentop= greenbottom + greenstep;
	if (bluebottom < 256)
	  bluetop= bluebottom + bluestep;

	xcolor.red= (redtop - 1) << 8;
	xcolor.green= (greentop - 1) << 8;
	xcolor.blue= (bluetop - 1) << 8;
	if (! XAllocColor(disp, xii->cmap, &xcolor)) {

	  /* if an allocation fails for a DirectColor default visual then
	   * we should create a private colormap and try again.
	   */

	  if ((visual->class == DirectColor) &&
	      (visual == DefaultVisual(disp, scrn))) {
	    xii->cmap= XCreateColormap(disp, RootWindow(disp, scrn),
					      visual, AllocNone);
	    goto retry_direct;
	  }

	  /* something completely unexpected happened
	   */

	  fprintf(stderr, "imageToXImage: XAllocColor failed on a TrueColor/Directcolor visual\n");
          lfree((byte *) redvalue);
          lfree((byte *) greenvalue);
          lfree((byte *) bluevalue);
          lfree((byte *) xii);
	  return(NULL);
	}

	/* fill in pixel values for each band at this intensity
	 */

	while ((redbottom < 256) && (redbottom < redtop))
	  redvalue[redbottom++]= xcolor.pixel & visual->red_mask;
	while ((greenbottom < 256) && (greenbottom < greentop))
	  greenvalue[greenbottom++]= xcolor.pixel & visual->green_mask;
	while ((bluebottom < 256) && (bluebottom < bluetop))
	  bluevalue[bluebottom++]= xcolor.pixel & visual->blue_mask;
      }
    }
    break;

  default:	/* Not TrueColor or DirectColor */
  retry: /* this tag is used when retrying because we couldn't get a fit */
    xii->index= (Pixel *)lmalloc(sizeof(Pixel) * (image->rgb.used+NOFLASH_COLORS));

    /* private_cmap flag is invalid if not a dynamic visual
     */

    switch (visual->class) {
    case StaticColor:
    case StaticGray:
      private_cmap= 0;
    }

    /* get the colormap to use.
     */

    if (private_cmap) { /* user asked us to use a private cmap */
      newmap= 1;
      fit= 0;
    }
    else if ((visual == DefaultVisual(disp, scrn)) ||
	     (visual->class == StaticGray) ||
	     (visual->class == StaticColor)) {

      /* if we're using the default visual, try to alloc colors shareable.
       * otherwise we're using a static visual and should treat it
       * accordingly.
       */

      if (visual == DefaultVisual(disp, scrn))
	xii->cmap= DefaultColormap(disp, scrn);
      else
	xii->cmap= XCreateColormap(disp, RootWindow(disp, scrn),
					  visual, AllocNone);
      newmap= 0;

      /* allocate colors shareable (if we can)
       */

      for (a= 0; a < image->rgb.used; a++) {
        if (dogamma) {
	  xcolor.red= GAMMA16(*(image->rgb.red + a));
	  xcolor.green= GAMMA16(*(image->rgb.green + a));
	  xcolor.blue= GAMMA16(*(image->rgb.blue + a));
        }
        else {
	  xcolor.red= *(image->rgb.red + a);
	  xcolor.green= *(image->rgb.green + a);
	  xcolor.blue= *(image->rgb.blue + a);
        }
	if (! XAllocColor(disp, xii->cmap, &xcolor)) {
	  if ((visual->class == StaticColor) ||
	      (visual->class == StaticGray)) {
	    printf("imageToXImage: XAllocColor failed on a static visual\n");
	    xii->no = a;
            freeXImage(image, xii);
	    return(NULL);
	  }
	  else {

	    /* we can't allocate the colors shareable so free all the colors
	     * we had allocated and create a private colormap (or fit
	     * into the default cmap if `fit' is true).
	     */

	    XFreeColors(disp, xii->cmap, xii->index, a, 0);
            xii->no = 0;
	    newmap= 1;
	    break;
	  }
        }
	*(xii->index + a)= xcolor.pixel;
      }
      xii->no = a;	/* number of pixels allocated in default visual */
    }
    else {
      newmap= 1;
      fit= 0;
    }

    if (newmap) {
      if (fit) {
        /* fit the image into the map we have.  Fitting the
         * colors is hard, we have to:
         *  1. count the available colors using XAllocColorCells.
         *  2. reduce the depth of the image to fit.
         *  3. grab the server so no one can goof with the colormap.
         *  4. free the colors we just allocated.
         *  5. allocate the colors again shareable.
         *  6. ungrab the server and continue on our way.
         */

	if (verbose)
	  printf("  Fitting image into default colormap\n");

        for (a= 0; a < image->rgb.used; a++) /* count entries we got */
	  if (! XAllocColorCells(disp, xii->cmap, False, NULL, 0,
			         xii->index + a, 1))
	    break;

	if (a <= 2) {
          Image *dimage;
	  if (verbose) {
	    printf("  Cannot fit into default colormap, dithering...");
	    fflush(stdout);
	  }
	  dimage= dither(image, 0);
          if(dimage != image && orig_image != image)
            freeImage(image);
          image = dimage;
	  if (verbose)
	    printf("done\n");
	  fit= 0;
          if(a > 0)
            XFreeColors(disp, xii->cmap, xii->index, a, 0);
	  lfree((byte *) xii->index);
          xii->index = NULL;
	  goto retry;
	}

        if (a < image->rgb.used) {	/* if image has too many colors, reduce it */
          Image *dimage;
	  dimage= reduce(image, a, options->colordither, display_gamma, verbose);
          if(dimage != image && orig_image != image)
            freeImage(image);
          image = dimage;
          if(GAMMA_NOT_EQUAL(display_gamma,image->gamma)) { /* reduce may have changed it */
            make_gamma(display_gamma/image->gamma,gammamap);
            dogamma = TRUE;
            }
        }

	XGrabServer(disp);	/* stop someone else snarfing the colors */
        XFreeColors(disp, xii->cmap, xii->index, a, 0);
	for (a= 0; a < image->rgb.used; a++) {
          if(dogamma) {
	    xcolor.red= GAMMA16(*(image->rgb.red + a));
	    xcolor.green= GAMMA16(*(image->rgb.green + a));
	    xcolor.blue= GAMMA16(*(image->rgb.blue + a));
          }
          else {
	    xcolor.red= *(image->rgb.red + a);
	    xcolor.green= *(image->rgb.green + a);
	    xcolor.blue= *(image->rgb.blue + a);
          }

	  /* if this fails we're in trouble
	   */

	  if (! XAllocColor(disp, xii->cmap, &xcolor)) {
	    printf("XAllocColor failed while fitting colormap!\n");
	    XUngrabServer(disp);
	    xii->no = a;
            freeXImage(image, xii);
	    return(NULL);
	  }
	  *(xii->index + a)= xcolor.pixel;
	}
	XUngrabServer(disp);
	xii->no = a;

      } else {	/* not fit */
      /*
       * we create a private cmap and allocate
       * the colors writable.
       */
	if (verbose)
	  printf("  Using private colormap\n");

	/* create new colormap
	 */

	xii->cmap= XCreateColormap(disp, RootWindow(disp, scrn),
					  visual, AllocNone);
        for (a= 0; a < image->rgb.used; a++) /* count entries we got */
	  if (! XAllocColorCells(disp, xii->cmap, False, NULL, 0,
			         xii->index + a, 1))
	    break;

        if (a == 0) {
	  fprintf(stderr, "imageToXImage: Color allocation failed!\n");
	  lfree((byte *) xii->index);
          xii->index = NULL;
          freeXImage(image, xii);
	  return(NULL);
        }

        if (a < image->rgb.used) {	/* if image has too many colors, reduce it */
          Image *dimage;
	  dimage= reduce(image, a, options->colordither, display_gamma, verbose);
          if(dimage != image && orig_image != image)
            freeImage(image);
          image = dimage;
          if(GAMMA_NOT_EQUAL(display_gamma,image->gamma)) { /* reduce may have changed it */
            make_gamma(display_gamma/image->gamma,gammamap);
            dogamma = TRUE;
            }
        }

	/* See if we can allocate some more colors for a few */
	/* entries from the default color map to reduce flashing.*/
	for(c=0; c < NOFLASH_COLORS; c++) {
	  if (! XAllocColorCells(disp, xii->cmap, False, NULL, 0,
			       xii->index + a + c, 1))
	    break;
	}
	for(b=0; b < c; b++) {	/* set default colors */
	  xcolor.pixel = *(xii->index + b);
	  XQueryColor(disp, DefaultColormap(disp, scrn), &xcolor);
	  XStoreColor(disp, xii->cmap, &xcolor);
	}
        if(dogamma) {
	  for (b=0; b < a; b++) {
	    xcolor.pixel= *(xii->index + c + b);
	    xcolor.red= GAMMA16(*(image->rgb.red + b));
	    xcolor.green= GAMMA16(*(image->rgb.green + b));
	    xcolor.blue= GAMMA16(*(image->rgb.blue + b));
	    XStoreColor(disp, xii->cmap, &xcolor);
          }
        }
        else {
	  for (b=0; b < a; b++) {
	    xcolor.pixel= *(xii->index + c + b);
	    xcolor.red= *(image->rgb.red + b);
	    xcolor.green= *(image->rgb.green + b);
	    xcolor.blue= *(image->rgb.blue + b);
	    XStoreColor(disp, xii->cmap, &xcolor);
          }
	}
      xii->no = a + c;
      }
    }
    break;
  }

  /* create an XImage and related colormap based on the image type
   * we have.
   */

  if (verbose) {
    printf("  Building XImage...");
    fflush(stdout);
  }


  switch (image->type) {
  case IBITMAP:
    {
      byte *dst, *src;
      int rowbytes;

      createImage(xii, image, visual, 1, XYBitmap);
      dst = xii->ximage->data;
      src = image->data;
      rowbytes = (image->width + 7) / 8;
      assert(rowbytes <= xii->ximage->bytes_per_line);
      for (y = image->height; y--;) {
	memcpy(dst, src, rowbytes);
	if (LSBFirst == xii->ximage->bitmap_bit_order)
	   flipBits(dst, rowbytes);
	dst += xii->ximage->bytes_per_line;
	src += rowbytes;
      }
      
      xii->depth= ddepth;
      if(visual->class == DirectColor || visual->class == TrueColor) {
        Pixel pixval;
        dbits= bitsPerPixelAtDepth(disp, scrn, ddepth);
        dpixlen= (dbits + 7) / 8;
        pixval= redvalue[image->rgb.red[0] >> 8] |
                greenvalue[image->rgb.green[0] >> 8] |
                bluevalue[image->rgb.blue[0] >> 8];
        xii->background = pixval;
        pixval= redvalue[image->rgb.red[1] >> 8] |
                greenvalue[image->rgb.green[1] >> 8] |
                bluevalue[image->rgb.blue[1] >> 8];
        xii->foreground = pixval;
      }
      else {	/* Not Direct or True Color */
        xii->foreground= *((xii->index + c) + 1);
        xii->background= *(xii->index + c);
      }
      break;
    }

  case IRGB:
  case ITRUE:

    /* modify image data to match visual and colormap
     */

    dbits= bitsPerPixelAtDepth(disp, scrn, ddepth);	/* bits per pixel */
    xii->depth= ddepth;
    dpixlen= (dbits + 7) / 8;	/* bytes per pixel */

    switch (visual->class) {
    case DirectColor:
    case TrueColor:
      { byte *data, *dst, *src;
	Pixel *pixels, *p;

	createImage(xii, image, visual, ddepth, ZPixmap);
	pixels = (Pixel *) lmalloc(image->width * sizeof(Pixel));

	src = image->data;
	data = xii->ximage->data;
	for (y = image->height; y--;) {
	  for (p = pixels, x = image->width; x--; src += image->pixlen, ++p)
	    *p = memToVal(src, image->pixlen);
	  if (IRGB == image->type) {
	    for (p = pixels, x = image->width; x--; ++p) {
              *p = RGB_TO_TRUE(image->rgb.red[*p], image->rgb.green[*p],
		image->rgb.blue[*p]);
	    }
	  }
	  if (dogamma) {
	    for (p = pixels, x = image->width; x--; ++p) {
	      *p = RGB_TO_TRUE(GAMMA8(TRUE_RED(*p)) << 8,
		GAMMA8(TRUE_GREEN(*p)) << 8, GAMMA8(TRUE_BLUE(*p)) << 8);
	    }
	  }
	  for (p = pixels, x = image->width; x--; ++p) {
	    *p = redvalue[TRUE_RED(*p)] | greenvalue[TRUE_GREEN(*p)] |
              bluevalue[TRUE_BLUE(*p)];
	  }
	  dst = data;
	  if (MSBFirst == xii->ximage->byte_order) {
	    for (p = pixels, x = image->width; x--; ++p, dst += dpixlen)
	      valToMem(*p, dst, dpixlen);
	  } else {
	    for (p = pixels, x = image->width; x--; ++p, dst += dpixlen)
	      valToMemLSB(*p, dst, dpixlen);
	  }
	  data += xii->ximage->bytes_per_line;
	}
	lfree((byte *) pixels);
        break;
      }

    default:

      /* only IRGB images make it this far.
       */

      createImage(xii, image, visual, ddepth, ZPixmap);

      /* if our XImage doesn't have modulus 8 bits per pixel, it's unclear
       * how to pack bits so we use XPutPixel.  this is slower.
       */

      if (dbits % 8) {
	byte *src;

	src = image->data;
        for (y = 0; y < image->height; ++x) {
	  for (x = 0; x < image->width; ++x) {
	    XPutPixel(xii->ximage, x, y,
	      xii->index[c + memToVal(src, image->pixlen)]);
	    src += image->pixlen;
	  }
	}
      } else {
	byte *data, *src, *dst;

        data = xii->ximage->data;
	src = image->data;
	for (y = image->height; y--;) {
	  dst = data;
	  if (MSBFirst == xii->ximage->byte_order) {
            for (x = image->width; x--; src += image->pixlen, dst += dpixlen) {
	      valToMem(xii->index[c + memToVal(src, image->pixlen)], dst,
		dpixlen);
            }
	  } else {
            for (x = image->width; x--; src += image->pixlen, dst += dpixlen) {
	      valToMemLSB(xii->index[c + memToVal(src, image->pixlen)], dst,
		dpixlen);
            }
	  }
	  data += xii->ximage->bytes_per_line;
	}
      }
      break;
    }
  }

  if (verbose)
    printf("done\n");

  if (redvalue) {
    lfree((byte *)redvalue);
    lfree((byte *)greenvalue);
    lfree((byte *)bluevalue);
  }
  if (verbose && dogamma)
    printf("  Have adjusted image from %4.2f to display gamma of %4.2f\n",image->gamma,display_gamma);
  if (image != orig_image)
    freeImage(image);
  return(xii);
}

/* Given an XImage and a drawable, move a rectangle from the Ximage
 * to the drawable.
 */

void sendXImage(XImageInfo *xii, int src_x, int src_y, int dst_x, int dst_y,
	unsigned w, unsigned h)
{
  XGCValues gcv;

  /* build and cache the GC */

  if (!xii->gc) {
    gcv.function= GXcopy;
    if (xii->ximage->depth == 1) {
      gcv.foreground= xii->foreground;
      gcv.background= xii->background;
      xii->gc= XCreateGC(xii->disp, xii->drawable,
	GCFunction | GCForeground | GCBackground, &gcv);
    }
    else
      xii->gc= XCreateGC(xii->disp, xii->drawable, GCFunction, &gcv);
  }

  if (src_x < 0 || src_y < 0 ||
      src_x + w > xii->ximage->width || src_y + h > xii->ximage->height)
    return;

  if (xii->shm.shmid >= 0) {
    XShmPutImage(xii->disp, xii->drawable, xii->gc,
      xii->ximage, src_x, src_y, dst_x, dst_y, w, h, False);
    XSync(xii->disp, False);
  } else {
    XPutImage(xii->disp, xii->drawable, xii->gc,
      xii->ximage, src_x, src_y, dst_x, dst_y, w, h);
  }
}

/* free up anything cached in the local Ximage structure.
 */

void freeXImage(Image *image, XImageInfo *xii)
{
  if (xii->index != NULL) {	/* if we allocated colors */
    if (xii->no > 0 && !xii->rootimage)	/* don't free root colors */
      XFreeColors(xii->disp, xii->cmap, xii->index, xii->no, 0);
    lfree((byte *) xii->index);
  }
  if (xii->gc)
    XFreeGC(xii->disp, xii->gc);
  if (xii->shm.shmid >= 0) {
    XShmDetach(xii->disp, &xii->shm);
    shmdt(xii->shm.shmaddr);
    shmctl(xii->shm.shmid, IPC_RMID, 0);
  } else {
    lfree((byte *) xii->ximage->data);
  }
  xii->ximage->data= NULL;
  XDestroyImage(xii->ximage);
  lfree((byte *) xii);
  /* should we free private color map to ??? */
}
