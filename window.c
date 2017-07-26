/*
 * window.c:
 *
 * display an image in a window
 *
 * jim frost 10.03.89
 *
 * Copyright 1989, 1990, 1991 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */

#include "copyright.h"
#include "xli.h"
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#if defined(linux)
#include <sys/time.h>
#endif
#if (defined(SYSV) || defined(SVR4)) && !defined(__hpux) && !defined(_CRAY)
#include <stropts.h>
#include <poll.h>
#endif
#ifdef _AIX
#include <sys/select.h>
#endif

static Window ImageWindow = 0;
static Window ViewportWin = 0;
static Colormap ImageColormap;

static int AlarmWentOff = 0;

static void delayAlarmHandler(int sig_no)
{
	AlarmWentOff = 1;
}

/* this is a bogus function whose only purpose is to interrupt
 * the XNextEvent signal call in imageInWindow().
 * This is added to allow automatic cycling through the specified list
 * of pictures. The amount of wait time is specified using the -delay
 * option, which is the number of seconds to pause between pictures.
 * - mfc 90/10/08
 */

static int getNextEventWithTimeout(Display *disp, XEvent *event)
{

#if (defined(SYSV) || defined(SVR4)) && !defined(__hpux) && !defined(_CRAY)
	struct pollfd pfd[1];
#else
	fd_set rmask;
	int nfound;
#endif

	/* force any output to occur before we set & spin
	 */

	XFlush(disp);

	/* wait for alarm
	 */

	while ((AlarmWentOff == 0)) {
		if (XPending(disp)) {
			XNextEvent(disp, event);
			return (1);
		}
#if (defined(SYSV) || defined(SVR4)) && !defined(__hpux) && !defined(_CRAY)
		pfd[0].fd = ConnectionNumber(disp);
		pfd[0].events = POLLIN;
		poll(pfd, 1, -1L);	/* use very long timeout */
		switch (pfd[0].revents) {
#else
		FD_ZERO(&rmask);
		FD_SET(ConnectionNumber(disp), &rmask);
		nfound = select(ConnectionNumber(disp) + 1, &rmask,
		  (fd_set *) 0, (fd_set *) 0, /*(struct timeval *) */ 0);
		switch (nfound) {
#endif
		case -1:
			if (errno == EINTR) {
				continue;
			} else {
				perror("select");
				continue;
			}
		}
	}			/* end while ! AlarmWentOff */
	return (0);
}

static void setCursor(Display *disp, Window window, unsigned int iw, unsigned int ih, unsigned int ww, unsigned int wh, Cursor *cursor)
{
	XSetWindowAttributes swa;

	if ((ww >= iw) && (wh >= ih))
		swa.cursor = XCreateFontCursor(disp, XC_icon);
	else if ((ww < iw) && (wh >= ih))
		swa.cursor = XCreateFontCursor(disp, XC_sb_h_double_arrow);
	else if ((ww >= iw) && (wh < ih))
		swa.cursor = XCreateFontCursor(disp, XC_sb_v_double_arrow);
	else
		swa.cursor = XCreateFontCursor(disp, XC_fleur);
	XChangeWindowAttributes(disp, window, CWCursor, &swa);
	XFreeCursor(disp, *cursor);
	*cursor = swa.cursor;
}

/*
 * place an image - ie. constain it to be as
 * visible as possible in the window, then move it there.
 */

static void placeImage(Display *disp, int width, int height, int winwidth, int winheight, int *rx, int *ry)
              
                                       
             			/* supplied and returned */
{
	int pixx, pixy;

	pixx = *rx;
	pixy = *ry;

	if (winwidth > width)	/* If window bigger than image */
		pixx = (winwidth - width) / 2;	/* center it */
	else {
		if ((pixx < 0) && (pixx + width < winwidth))	/* if too far left */
			pixx = winwidth - width;	/* constrain to right hand side */
		if (pixx > 0)	/* if too far right */
			pixx = 0;	/* constrain to left hand side */
	}
	if (winheight > height)
		pixy = (winheight - height) / 2;
	else {
		if ((pixy < 0) && (pixy + height < winheight))
			pixy = winheight - height;
		if (pixy > 0)
			pixy = 0;
	}
	*rx = pixx;
	*ry = pixy;
	XMoveWindow(disp, ImageWindow, pixx, pixy);
}

/* blit an image
 */

static void blitImage(XImageInfo *ximageinfo, unsigned int width, unsigned int height, int x, int y, int w, int h)
{
	if (w > width)
		w = width;
	if (h > height)
		h = height;
	if (x < 0) {
		XClearArea(ximageinfo->disp, ximageinfo->drawable, x, y, -x, h, FALSE);
		w -= (0 - x);
		x = 0;
	}
	if (y < 0) {
		XClearArea(ximageinfo->disp, ximageinfo->drawable, x, y, w, -y, FALSE);
		h -= (0 - y);
		y = 0;
	}
	if (x + w > width) {
		XClearArea(ximageinfo->disp, ximageinfo->drawable,
			   x + width, y, x + w - width, h, FALSE);
		w -= x + w - width;
	}
	if (y + h > height) {
		XClearArea(ximageinfo->disp, ximageinfo->drawable,
			   x, y + height, w, y + h - height, FALSE);
		h -= y + h - height;
	}
	sendXImage(ximageinfo, x, y, x, y, w, h);
}

/* clean up static window if we're through with it
 */

void cleanUpWindow(DisplayInfo *dinfo)
{
	Display *disp = dinfo->disp;
	if (ImageWindow)
		XDestroyWindow(disp, ImageWindow);
	ImageWindow = 0;
	if (ViewportWin)
		XDestroyWindow(disp, ViewportWin);
	ViewportWin = 0;
}

/* clean up after displaying an image
 */

static void cleanUpImage(Display *disp, int scrn, Cursor cursor, Pixmap pixmap, Image *image, XImageInfo *ximageinfo)
{
	XFreeCursor(disp, cursor);
	if (pixmap != None)
		XFreePixmap(disp, pixmap);
	freeXImage(image, ximageinfo);
}

/* this sets the colormap and WM_COLORMAP_WINDOWS properly for the
 * viewport.
 */

static void setViewportColormap(Display *disp, int scrn, Visual *visual)
{
	XSetWindowAttributes swa;
	static int cmap_atom = None;
	Window cmap_windows[2];

	if (cmap_atom == None)
		cmap_atom = XInternAtom(disp, "WM_COLORMAP_WINDOWS", FALSE);

	/* if the visual we're using is the same as the default visual (used by
	 * the viewport window) then we can set the viewport window to use the
	 * image's colormap.  this keeps most window managers happy.
	 */

	if (visual == DefaultVisual(disp, scrn)) {
		swa.colormap = ImageColormap;
		XChangeWindowAttributes(disp, ViewportWin, CWColormap, &swa);
		XDeleteProperty(disp, ViewportWin, cmap_atom);
	}
	/* smart window managers can handle it when we use a different colormap
	 * in our subwindow so long as we set the WM_COLORMAP_WINDOWS property
	 * ala ICCCM.
	 */

	else {
		cmap_windows[0] = ImageWindow;
		cmap_windows[1] = ViewportWin;
		XChangeProperty(disp, ViewportWin, cmap_atom, XA_WINDOW, 32,
		     PropModePrepend, (unsigned char *) cmap_windows, 2);
	}
}

/* this attempts to convert an image title into a reasonable icon name
 */

static char *iconName(char *s)
{
	static char buf[BUFSIZ];
	char *t;

	if (!s)
		return ("Unnamed");
	buf[BUFSIZ - 1] = '\0';
	strncpy(buf, s, BUFSIZ - 1);
	t = index(buf, ' ');	/* strip off stuff following 1st word.  this strips */
	if (t)			/* info added by processing functions too. */
		*t = '\0';

	/* strip off leading path.  if you don't use unix-style paths, you might
	 * want to change this.
	 */

	if ((t = rindex(buf, '/'))) {
		for (s = buf, t++; *t; s++, t++)
			*s = *t;
		*s = '\0';
	}
	t = index(buf, '.');	/* look for an extension and strip it off */
	if (t)
		*t = '\0';
	return (buf);
}

/* find the best visual of a particular class with a particular depth
 */

static Visual *bestVisualOfClassAndDepth(Display *disp, int scrn, int class, unsigned int depth)
{
	Visual *best = 0;
	XVisualInfo template, *info;
	int nvisuals;

	template.screen = scrn;
	template.class = class;
	template.depth = depth;
	if (!(info = XGetVisualInfo(disp, VisualScreenMask | VisualClassMask |
				 VisualDepthMask, &template, &nvisuals)))
		return (Visual *) 0;	/* no visuals of this depth */

	/* not sure what to do if this gives more than one visual of a particular
	 * class and depth, so just return the first one.
	 */

	best = info->visual;
	XFree((caddr_t) info);
	return (best);
}

/* this tries to determine the best available visual to use for a particular
 * image
 */

static void bestVisual(Display *disp, int scrn, Image *image, Visual **rvisual, unsigned int *rdepth)
{
	unsigned int depth, a;
	Screen *screen;
	Visual *visual, *default_visual;

	/* figure out the best depth the server supports.  note that some servers
	 * (such as the HP 11.3 server) actually say they support some depths but
	 * have no visuals that support that depth.  seems silly to me....
	 */

	depth = 0;
	screen = ScreenOfDisplay(disp, scrn);
	for (a = 0; a < screen->ndepths; a++) {
		if (screen->depths[a].nvisuals &&
		    ((!depth
		      || ((depth < image->depth) && (screen->depths[a].depth > depth))
		      || ((screen->depths[a].depth >= image->depth)
			  && (screen->depths[a].depth < depth)))))
			depth = screen->depths[a].depth;
	}
	if (!depth) {		/* this shouldn't happen */
		printf("bestVisual: didn't find any depths?!?\n");
		depth = DefaultDepth(disp, scrn);
	}
	/* given this depth, find the best possible visual
	 */

	default_visual = DefaultVisual(disp, scrn);
	visual = 0;
	switch (image->type) {
	case ITRUE:

		/* if the default visual is DirectColor or TrueColor prioritize such
		 * that we use the default type if it exists at this depth
		 */

		if (default_visual->class == TrueColor) {
			visual = bestVisualOfClassAndDepth(disp, scrn, TrueColor, depth);
			if (!visual)
				visual = bestVisualOfClassAndDepth(disp, scrn, DirectColor, depth);
		} else {
			visual = bestVisualOfClassAndDepth(disp, scrn, DirectColor, depth);
			if (!visual)
				visual = bestVisualOfClassAndDepth(disp, scrn, TrueColor, depth);
		}

		if (!visual || ((depth <= 8) &&
				bestVisualOfClassAndDepth(disp, scrn, PseudoColor, depth)))
			visual = bestVisualOfClassAndDepth(disp, scrn, PseudoColor, depth);
		if (!visual)
			visual = bestVisualOfClassAndDepth(disp, scrn, StaticColor, depth);
		if (!visual)
			visual = bestVisualOfClassAndDepth(disp, scrn, GrayScale, depth);
		if (!visual)
			visual = bestVisualOfClassAndDepth(disp, scrn, StaticGray, depth);
		break;

	case IRGB:

		/* if it's an RGB image, we want PseudoColor if we can get it
		 */

		visual = bestVisualOfClassAndDepth(disp, scrn, PseudoColor, depth);
		if (!visual)
			visual = bestVisualOfClassAndDepth(disp, scrn, DirectColor, depth);
		if (!visual)
			visual = bestVisualOfClassAndDepth(disp, scrn, TrueColor, depth);
		if (!visual)
			visual = bestVisualOfClassAndDepth(disp, scrn, StaticColor, depth);
		if (!visual)
			visual = bestVisualOfClassAndDepth(disp, scrn, GrayScale, depth);
		if (!visual)
			visual = bestVisualOfClassAndDepth(disp, scrn, StaticGray, depth);
		break;

	case IBITMAP:
		visual = bestVisualOfClassAndDepth(disp, scrn, PseudoColor, depth);
		if (!visual)
			visual = bestVisualOfClassAndDepth(disp, scrn, StaticColor, depth);
		if (!visual)
			visual = bestVisualOfClassAndDepth(disp, scrn, GrayScale, depth);
		if (!visual)
			visual = bestVisualOfClassAndDepth(disp, scrn, StaticGray, depth);

		/* it seems pretty wasteful to use a TrueColor or DirectColor visual
		 * to display a bitmap (2-color) image, so we look for those last
		 */

		if (!visual)
			visual = bestVisualOfClassAndDepth(disp, scrn, DirectColor, depth);
		if (!visual)
			visual = bestVisualOfClassAndDepth(disp, scrn, TrueColor, depth);
		break;
	}

	if (!visual) {		/* this shouldn't happen */
		fprintf(stderr, "bestVisual: couldn't find one?!?\n");
		depth = DefaultDepth(disp, scrn);
		visual = DefaultVisual(disp, scrn);
	}
	*rvisual = visual;
	*rdepth = depth;
}

/* given a visual class, try to find the best visual of that class at
 * the best depth.  returns a null visual and depth if it couldn't find
 * any visual of that type at any depth
 * (This is used when the user has specified a visual class)
 */

static void bestVisualOfClass(Display *disp, int scrn, Image *image, int visual_class, Visual **rvisual, unsigned int *rdepth)
{
	Visual *visual;
	Screen *screen;
	unsigned int a, b, depth;

	/* loop through depths looking for a visual of a good depth which matches
	 * our visual class.
	 */

	screen = ScreenOfDisplay(disp, scrn);
	visual = (Visual *) 0;
	depth = 0;
	for (a = 0; a < screen->ndepths; a++) {
		for (b = 0; b < screen->depths[a].nvisuals; b++) {
			if ((screen->depths[a].visuals[b].class == visual_class) &&
			    (!depth ||
			     ((depth < image->depth) && (screen->depths[a].depth > depth)) ||
			     ((screen->depths[a].depth >= image->depth) &&
			      (screen->depths[a].depth < depth)))) {
				depth = screen->depths[a].depth;
				visual = &(screen->depths[a].visuals[b]);
			}
		}
	}
	*rvisual = visual;
	*rdepth = depth;
}

char imageInWindow(DisplayInfo *dinfo, Image *image, ImageOptions *options, int argc, char **argv)
{
	Display *disp = dinfo->disp;
	int scrn = dinfo->scrn;
	unsigned int private_cmap = globals.private_cmap;
	Pixmap pixmap = None;
	XImageInfo *xii;
	Visual *visual;
	unsigned int depth;
	int delay = options->delay;
	Window oldimagewindow;
	Colormap oldcmap;
	XSetWindowAttributes swa_img;
	XSetWindowAttributes swa_view;
	XClassHint classhint;
	unsigned int wa_mask_img;
	XSizeHints sh;
	XWMHints wmh;
	int pixx = -1, pixy = -1;
	int lastx, lasty, mousex, mousey;
	int paint;
	static int old_width = -1, old_height = -1;
	static Atom proto_atom = None, delete_atom = None;
	union {
		XEvent event;
		XAnyEvent any;
		XButtonEvent button;
		XKeyEvent key;
		XConfigureEvent configure;
		XExposeEvent expose;
		XMotionEvent motion;
		XResizeRequestEvent resize;
		XClientMessageEvent message;
	} event;
	int winx, winy, winwidth, winheight;
	int user_geometry;

	oldimagewindow = None;
	oldcmap = None;
	lastx = lasty = -1;

	/* figure out the window size.  unless specifically requested to do so,
	 * we will not exceed 90% of display real estate.
	 */

	if (!globals.user_geometry) {
		winx = winy = winwidth = winheight = 0;
		user_geometry = 0;
	} else {
		user_geometry = XGeometry(disp, scrn, globals.user_geometry,
			"0x0+0+0", 0, 1, 1, 0, 0, &winx, &winy,
			&winwidth, &winheight);
	}

	if (globals.fullscreen || globals.fillscreen) {
		winwidth = DisplayWidth(disp, scrn);
		winheight = DisplayHeight(disp, scrn);
	} else {
		lastx = (winwidth || winheight);  /* user set size flag */
		if (!winwidth) {
			winwidth = image->width;
			if (winwidth > DisplayWidth(disp, scrn) * 0.9)
				winwidth = DisplayWidth(disp, scrn) * 0.9;
		}
		if (!winheight) {
			winheight = image->height;
			if (winheight > DisplayHeight(disp, scrn) * 0.9)
				winheight = DisplayHeight(disp, scrn) * 0.9;
		}
	}

	/* if the user told us to fit the colormap, we must use the default
	 * visual.
	 */

	if (globals.fit) {
		visual = DefaultVisual(disp, scrn);
		depth = DefaultDepth(disp, scrn);
	} else {

		visual = (Visual *) 0;
		if (globals.visual_class == -1) {
			/* try to pick the best visual for the image. */

			bestVisual(disp, scrn, image, &visual, &depth);
			if (globals.verbose && (visual != DefaultVisual(disp,
					scrn)))
				printf("  Using %s visual\n", nameOfVisualClass(visual->class));
		} else {
			/* try to find a visual of the specified class */

			bestVisualOfClass(disp, scrn, image,
				globals.visual_class, &visual, &depth);
			if (!visual) {
				bestVisual(disp, scrn, image, &visual, &depth);
				fprintf(stderr, "Server does not support %s visual, using %s\n",
				 nameOfVisualClass(globals.visual_class),
					nameOfVisualClass(visual->class));
			}
		}
	}

	/* if we're in slideshow mode and the user told us to fit the colormap,
	 * free it here.
	 */

	if (ViewportWin) {
		if (globals.fit) {
			XDestroyWindow(disp, ImageWindow);
			ImageWindow = 0;
			ImageColormap = 0;
		}
		/* for the 1st image we display we can use the default cmap.
		 * subsequent images use a private colormap (unless they're
		 * bitmaps or grey scale) so we don't get color erosion when
		 * switching images.
		 */

		else if (!options->gray && !BITMAPP(image))
			private_cmap = 1;
	}
	if (!(xii = imageToXImage(disp, scrn, visual, depth, image,
			private_cmap, globals.fit, options))) {
		fprintf(stderr, "Cannot convert Image to XImage\n");
		exit(1);
	}
	swa_view.background_pixel = WhitePixel(disp, scrn);
	swa_view.backing_store = NotUseful;
	swa_view.cursor = XCreateFontCursor(disp, XC_watch);
	swa_view.event_mask = ButtonPressMask | Button1MotionMask
		| KeyPressMask | StructureNotifyMask | EnterWindowMask
		| LeaveWindowMask;
	swa_view.save_under = FALSE;

	classhint.res_class = "xli";
	classhint.res_name = "xli";
	if (!ViewportWin) {
		ViewportWin = XCreateWindow(disp, RootWindow(disp, scrn),
			winx, winy, winwidth, winheight, 0,
			DefaultDepth(disp, scrn), InputOutput,
			DefaultVisual(disp, scrn),
			CWBackingStore | CWBackPixel | CWCursor
			| CWEventMask | CWSaveUnder, &swa_view);
		oldimagewindow = 0;
		XSetCommand(disp, ViewportWin, argv, argc);
		XSetClassHint(disp, ViewportWin, &classhint);
		proto_atom = XInternAtom(disp, "WM_PROTOCOLS", FALSE);
		delete_atom = XInternAtom(disp, "WM_DELETE_WINDOW", FALSE);
		if ((proto_atom != None) && (delete_atom != None)) {
			XChangeProperty(disp, ViewportWin, proto_atom, XA_ATOM,
				32, PropModePrepend,
				(unsigned char *) &delete_atom, 1);
		}
		if (globals.focus) {
			XSetTransientForHint(disp, ViewportWin,
				atoi(getenv("WINDOWID")));
		}
		paint = 0;
	} else {
		oldimagewindow = ImageWindow;
		oldcmap = ImageColormap;
		paint = 1;
	}

	/* create image window */

	swa_img.bit_gravity = NorthWestGravity;
	swa_img.save_under = FALSE;
	swa_img.colormap = xii->cmap;
	swa_img.border_pixel = 0;
	swa_img.event_mask = KeyPressMask;	/* (Some systems need this) */
	ImageWindow = XCreateWindow(disp, ViewportWin, winx, winy,
		image->width, image->height, 0, xii->depth, InputOutput, visual,
		CWBitGravity | CWColormap | CWSaveUnder | CWBorderPixel
		| CWEventMask, &swa_img);
	ImageColormap = xii->cmap;
	XSetCommand(disp, ImageWindow, argv, argc);
	XSetClassHint(disp, ImageWindow, &classhint);

	/* decide how we're going to handle repaints.  we have three modes:
	 * use backing-store, use background pixmap, and use exposures.
	 * if the server supports backing-store, we enable it and use it.
	 * this really helps servers which are memory constrained.  if the
	 * server does not have backing-store, we try to send the image to
	 * a pixmap and use that as backing-store.  if that fails, we use
	 * exposures to blit the image (which is ugly but it works).
	 *
	 * the "use_pixmap" flag forces background pixmap mode, which may
	 * improve performance.
	 */

	xii->drawable = ImageWindow;
	if ((DoesBackingStore(ScreenOfDisplay(disp, scrn)) == NotUseful &&
	     xii->shm.shmid < 0) || globals.use_pixmap) {
		if (((pixmap = ximageToPixmap(disp, ImageWindow, xii)) ==
		     None) && globals.verbose)
			printf("  Cannot create image in server, repaints will be ugly!\n");
	}
	/* build window attributes for the image window
	 */

	wa_mask_img = 0;
	if (pixmap == None) {

		/* No pixmap.  Must paint over the wire.  Ask for BackingStore
		 * to cut down on the painting.  But, ask for Exposures so we
		 * can paint both viewables and backingstore.
		 */

		swa_img.background_pixel = WhitePixel(disp, scrn);
		wa_mask_img |= CWBackPixel;
		swa_img.event_mask |= ExposureMask;
		wa_mask_img |= CWEventMask;
		if (xii->shm.shmid < 0) {
			swa_img.backing_store = WhenMapped;
			wa_mask_img |= CWBackingStore;
		}
	} else {

		/* we have a pixmap so tile the window.  to move the image we only
		 * have to move the window and the server should do the rest.
		 */

		swa_img.background_pixmap = pixmap;
		wa_mask_img |= CWBackPixmap;
		swa_img.event_mask &= ~ExposureMask;	/* no exposures please */
		wa_mask_img |= CWEventMask;
		swa_img.backing_store = NotUseful;
		wa_mask_img |= CWBackingStore;
	}
	XChangeWindowAttributes(disp, ImageWindow, wa_mask_img, &swa_img);

	if (options->title) {	/* Option overides title */
		XStoreName(disp, ViewportWin, options->title);
		XSetIconName(disp, ViewportWin, iconName(options->title));
	} else if (image->title) {
		XStoreName(disp, ViewportWin, image->title);
		XSetIconName(disp, ViewportWin, iconName(image->title));
	} else {
		XStoreName(disp, ViewportWin, "Unnamed");
		XSetIconName(disp, ViewportWin, iconName((char *) 0));
	}

	sh.width = winwidth;
	sh.height = winheight;
	if (globals.fullscreen || globals.fillscreen) {
		sh.min_width = sh.max_width = winwidth;
		sh.min_height = sh.max_height = winheight;
	} else {
		sh.min_width = 1;
		sh.min_height = 1;
		sh.max_width = image->width;
		sh.max_height = image->height;
	}
	sh.width_inc = 1;
	sh.height_inc = 1;
	sh.flags = PMinSize | PMaxSize | PResizeInc;
	if (lastx || globals.fullscreen || globals.fillscreen)
		sh.flags |= USSize;
	else
		sh.flags |= PSize;
	if (globals.fullscreen || globals.fillscreen) {
		sh.x = sh.y = 0;
		sh.flags |= USPosition;
	} else if (user_geometry & (XValue | YValue)) {
		sh.x = winx;
		sh.y = winy;
		sh.flags |= USPosition;
	}
	XSetNormalHints(disp, ViewportWin, &sh);
	sh.min_width = sh.max_width;
	sh.min_height = sh.max_height;
	XSetNormalHints(disp, ImageWindow, &sh);	/* Image doesn't shrink */

	wmh.input = TRUE;
	wmh.flags = InputHint;
	XSetWMHints(disp, ViewportWin, &wmh);

	setViewportColormap(disp, scrn, visual);

	/* map windows and clean up old window if there was one.
	 */

	XMapWindow(disp, ImageWindow);
	XMapWindow(disp, ViewportWin);
	if (oldimagewindow) {
		if (oldcmap && (oldcmap != DefaultColormap(disp, scrn)))
			XFreeColormap(disp, oldcmap);
		XDestroyWindow(disp, oldimagewindow);
	}
	/* start displaying image
	 */

	placeImage(disp, image->width, image->height, winwidth, winheight, &pixx, &pixy);
	if (paint) {
		if ((winwidth != old_width) || (winheight != old_height)) {
			XResizeWindow(disp, ViewportWin, winwidth, winheight);
		}
		XResizeWindow(disp, ImageWindow, image->width, image->height);
		/* Clear the image window.  Ask for exposure if there is no tile. */
		XClearArea(disp, ImageWindow, 0, 0, 0, 0, (pixmap == None));
	}
	old_width = winwidth;
	old_height = winheight;

	/* flush output. this is so that -delay will be close to what was
	 * asked for (i.e., do the flushing of output outside of the loop).
	 */
	XSync(disp, FALSE);

	setCursor(disp, ViewportWin, image->width, image->height,
		  winwidth, winheight, &(swa_view.cursor));
	lastx = lasty = -1;
	if (delay > 0) {
		/* reset alarm to -delay seconds after every event */
		AlarmWentOff = 0;
		signal(SIGALRM, delayAlarmHandler);
		alarm(delay);
	}
	for (;;) {
		if (delay > 0) {
			if (!getNextEventWithTimeout(disp, &event.event)) {
				Cursor cursor = swa_view.cursor;

				/* timeout expired.  clean up and exit.
				 */

				swa_view.cursor = XCreateFontCursor(disp, XC_watch);
				XChangeWindowAttributes(disp, ImageWindow, CWCursor, &swa_view);
				XFreeCursor(disp, cursor);
				XFlush(disp);
				cleanUpImage(disp, scrn, swa_view.cursor, pixmap, image, xii);
				return ('n');
			}
		} else if (delay == 0 && !XPending(disp)) {
			Cursor cursor = swa_view.cursor;
			swa_view.cursor = XCreateFontCursor(disp, XC_watch);
			XChangeWindowAttributes(disp, ImageWindow, CWCursor, &swa_view);
			XFreeCursor(disp, cursor);
			XFlush(disp);
			cleanUpImage(disp, scrn, swa_view.cursor, pixmap, image, xii);
			return ('n');
		} else {
			XNextEvent(disp, &event.event);
		}

		switch (event.any.type) {
		case ButtonPress:
			if (event.button.button == 1) {
				lastx = event.button.x;
				lasty = event.button.y;
				break;
			}
			break;

		case KeyPress:{
				char buf[128];
				KeySym ks;
				XComposeStatus status;
				char ret;
				Cursor cursor;

				if (XLookupString(&event.key, buf, 128, &ks, &status) != 1)
					break;
				ret = buf[0];
				if (isupper(ret))
					ret = tolower(ret);
				switch (ret) {
				case ' ':
				case 'l':
				case 'r':
				case 'n':
				case 'f':
				case 'b':
				case 'p':
				case '.':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				case '0':
				case '<':
				case '=':
				case '>':
				case 'x':
					if ('x' == ret && !globals.delete)
						break;

					if (delay > 0)
						alarm(0);
					cursor = swa_view.cursor;
					swa_view.cursor = XCreateFontCursor(disp, XC_watch);
					XChangeWindowAttributes(disp, ViewportWin, CWCursor, &swa_view);
					XFreeCursor(disp, cursor);
					XFlush(disp);
					cleanUpImage(disp, scrn, swa_view.cursor, pixmap, image, xii);
					return (ret);
				case '\003':	/* ^C */
				case 'q':
					if (delay > 0)
						alarm(0);
					cleanUpImage(disp, scrn, swa_view.cursor, pixmap, image, xii);
					return (ret);
				}
				break;
			}

		case MotionNotify:
			if ((image->width <= winwidth) && (image->height <= winheight))
				break;	/* we're AT&T */
			mousex = event.button.x;
			mousey = event.button.y;
			/*XSync(disp, FALSE); */
			while (XCheckTypedEvent(disp, MotionNotify, (XEvent *) & event) == TRUE) {
				mousex = event.button.x;
				mousey = event.button.y;
			}
			pixx -= (lastx - mousex);
			pixy -= (lasty - mousey);
			lastx = mousex;
			lasty = mousey;
			placeImage(disp, image->width, image->height, winwidth, winheight,
				   &pixx, &pixy);
			break;

		case ConfigureNotify:
			winwidth = old_width = event.configure.width;
			winheight = old_height = event.configure.height;

			placeImage(disp, image->width, image->height, winwidth, winheight,
				   &pixx, &pixy);

			/* configure the cursor to indicate which directions we can drag
			 */

			setCursor(disp, ViewportWin, image->width, image->height,
				winwidth, winheight, &(swa_view.cursor));
			break;

		case DestroyNotify:
			cleanUpImage(disp, scrn, swa_view.cursor, pixmap, image, xii);
			return ('\0');

		case Expose:
			blitImage(xii, image->width, image->height,
				  event.expose.x, event.expose.y,
				event.expose.width, event.expose.height);
			break;

		case EnterNotify:
			if (globals.install)
				XInstallColormap(disp, xii->cmap);
			break;

		case LeaveNotify:
			if (globals.install)
				XUninstallColormap(disp, xii->cmap);
			break;

		case ClientMessage:
			/* if we get a client message for the viewport window which has the
			 * value of the delete atom, it means the window manager wants us to
			 * die.
			 */

			if ((event.message.window == ViewportWin) &&
			    (event.message.data.l[0] == delete_atom)) {
				cleanUpImage(disp, scrn, swa_view.cursor, pixmap, image, xii);
				return ('q');
			}
			break;
		}
	}
}
