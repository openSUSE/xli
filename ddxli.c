/*
 * General Device dependent code for xli.
 *
 * Author; Graeme W. Gill
 */

#include "xli.h"
#include <ctype.h>

static int saParseXColor(DisplayInfo *dinfo, char *spec, XColor *xcolor);

/* Initialise a display info structure to default values */
void xliDefaultDispinfo(DisplayInfo *dinfo)
{
	dinfo->width = 0;
	dinfo->height = 0;
	dinfo->disp = NULL;
	dinfo->scrn = 0;
	dinfo->colormap = 0;
}

/* open up a display and screen, and stick the info away */
/* Return FALSE on failure */
boolean xliOpenDisplay(DisplayInfo *dinfo, char *name)
{
	Display *disp;
	int scrn;
	if (!(disp = XOpenDisplay(globals.dname)))
		return FALSE;	/* failed */
	scrn = DefaultScreen(disp);
	dinfo->disp = disp;
	dinfo->scrn = scrn;
	dinfo->colormap = DefaultColormap(disp, scrn);
	dinfo->width = DisplayWidth(disp, scrn);
	dinfo->height = DisplayHeight(disp, scrn);
	XSetErrorHandler(errorHandler);

	return TRUE;
}

/* report display name when connection fails */
char *xliDisplayName(char *name)
{
	return XDisplayName(name);
}

void xliCloseDisplay(DisplayInfo *dinfo)
{
	XCloseDisplay(dinfo->disp);
}

/*
simple X11 error handler.  this provides us with some kind of error recovery.
*/

int errorHandler(Display * disp, XErrorEvent * error)
{
	char errortext[BUFSIZ];

	XGetErrorText(disp, error->error_code, errortext, BUFSIZ);
	fprintf(stderr, "xli: X Error: %s on 0x%lx\n",
		errortext, error->resourceid);
	if (globals._Xdebug)	
		abort();
	else
		return (0);
}

/* Return the default visual class */
int xliDefaultVisual(void)
{
	return (DefaultVisual(globals.dinfo.disp, globals.dinfo.scrn)->class);
}

/* return the default depth of the default visual */
int xliDefaultDepth(void)
{
	return DefaultDepth(globals.dinfo.disp, globals.dinfo.scrn);
}

/* Print some information about the display */
void tellAboutDisplay(DisplayInfo * dinfo)
{
	Screen *screen;
	int a, b;
	if (dinfo->disp) {
		screen = ScreenOfDisplay(dinfo->disp, dinfo->scrn);
		printf("Server: %s Version %d\n", ServerVendor(dinfo->disp), VendorRelease(dinfo->disp));
		printf("Depths and visuals supported:\n");
		for (a = 0; a < screen->ndepths; a++) {
			printf("%2d:", screen->depths[a].depth);
			for (b = 0; b < screen->depths[a].nvisuals; b++)
				printf(" %s", nameOfVisualClass(screen->depths[a].visuals[b].class));
			printf("\n");
		}
	} else {
		printf("[No information on server; error occurred before connection]\n");
	}
}

/* A standalone color name to RGB lookup routine */

typedef struct {
	char *name;
	unsigned char red;
	unsigned char green;
	unsigned char blue;
} xliXColorTableEntry;

#include "rgbtab.h"		/* Equivalent of an X color table */

/* parse an X like color. */
int xliParseXColor(DisplayInfo *dinfo, char *spec, XColor *xcolor)
{
	int stat;

	if (dinfo->disp) {
		stat = XParseColor(globals.dinfo.disp, globals.dinfo.colormap,
				   spec, xcolor);
	} else {
		stat = saParseXColor(dinfo, spec, xcolor);
	}

	return stat;
}

static int saParseXColor(DisplayInfo *dinfo, char *spec, XColor *xcolor)
{
	int i, n, r, g, b;
	xliXColorTableEntry *tp;

	xcolor->red = 0;
	xcolor->green = 0;
	xcolor->blue = 0;
	n = strlen(spec);
	if (n > 0) {
		for (i = 0; i < n; i++) {
			if (isupper(spec[i]))
				spec[i] = tolower(spec[i]);
		}
		if (spec[0] == '#') {
			switch (n - 1) {
			case 3:
				r = hstoi(spec + 1, 1);
				g = hstoi(spec + 2, 1);
				b = hstoi(spec + 3, 1);
				if (r >= 0 && g >= 0 && b >= 0) {
					xcolor->red = r * 0x1111;
					xcolor->green = g * 0x1111;
					xcolor->blue = b * 0x1111;

					return 1;
				}
				break;
			case 6:
				r = hstoi(spec + 1, 2);
				g = hstoi(spec + 3, 2);
				b = hstoi(spec + 5, 2);
				if (r >= 0 && g >= 0 && b >= 0) {
					xcolor->red = r * 0x101;
					xcolor->green = g * 0x101;
					xcolor->blue = b * 0x101;

					return 1;
				}
				break;
			case 9:
				r = hstoi(spec + 1, 3);
				g = hstoi(spec + 4, 3);
				b = hstoi(spec + 7, 3);
				if (r >= 0 && g >= 0 && b >= 0) {
					xcolor->red = (r * 65535) / 4095;
					xcolor->green = (g * 65535) / 4095;
					xcolor->blue = (b * 65535) / 4095;

					return 1;
				}
				break;
			case 12:
				r = hstoi(spec + 1, 4);
				g = hstoi(spec + 5, 4);
				b = hstoi(spec + 9, 4);
				if (r >= 0 && g >= 0 && b >= 0) {
					xcolor->red = r;
					xcolor->green = g;
					xcolor->blue = b;

					return 1;
				}
				break;
			}
		} else {
			/*
			else its a color name.  Do a linear search for it 
			(binary search would be better)
			*/
			for (n = 0, tp = &xliXColorTable[0]; tp->name[0]; tp++) {
				if (strcmp(spec, tp->name) == 0) {
					xcolor->red = tp->red * 0x101;
					xcolor->green = tp->green * 0x101;
					xcolor->blue = tp->blue * 0x101;

					return 1;
				}
			}
		}
	}

	/* default - return black */
	xcolor->red = 0;
	xcolor->green = 0;
	xcolor->blue = 0;

	return 0;
}


/* gamma correct an XColor */
void xliGammaCorrectXColor(XColor * xlicolor, double gamma)
{
	xlicolor->red = (int) (0.5 + 65535.0 * pow((double) xlicolor->red / 65535.0, gamma));
	xlicolor->green = (int) (0.5 + 65535.0 * pow((double) xlicolor->green / 65535.0, gamma));
	xlicolor->blue = (int) (0.5 + 65535.0 * pow((double) xlicolor->blue / 65535.0, gamma));
}
