/*
 * General Device dependent code for xli.
 *
 * Author; Graeme W. Gill
 */

/* OS dependent stuff */
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

#if defined(SYSV) || defined(VMS)
#include <string.h>
#ifndef index			/* some SysV's do this for you */
#define index strchr
#endif
#ifndef rindex
#define rindex strrchr
#endif
#ifndef HAS_MEMCPY
#define HAS_MEMCPY
#endif
#else				/* !SYSV && !VMS */
#include <strings.h>
#endif				/* !SYSV && !VMS */

#ifdef VMS
#define R_OK 4
#define NO_UNCOMPRESS
#endif

/* equate bcopy with memcpy and bzero with memset where appropriate. */
#ifdef HAS_MEMCPY
#ifndef bcopy
#define bcopy(S,D,N) memcpy((char *)(D),(char *)(S),(N))
#endif
#ifndef bzero
#define bzero(P,N) memset((P),'\0',(N))
#endif
#ifndef bfill
#define bfill(P,N,C) memset((P),(C),(N))
#endif
#else				/* !HAS_MEMCPY */
void bfill(char *s, int n, int c);
#endif				/* !HAS_MEMCPY */

/* xli specific data types */

/* If signed char is a legal declaration
 * else assume char is signed by default
 */
#define HAS_SIGNED_CHAR		

typedef unsigned long Pixel;	/* what X thinks a pixel is */
typedef unsigned short Intensity;	/* what X thinks an RGB intensity is */
typedef unsigned char byte;	/* unsigned byte type */

/* signed byte type */
#ifdef HAS_SIGNED_CHAR
typedef signed char sbyte;
#else
typedef char sbyte;
#endif

#ifndef HAVE_BOOLEAN
#define HAVE_BOOLEAN
typedef int boolean;
#endif

#ifndef FALSE
#define FALSE 0
#define TRUE (!FALSE)
#endif

/* Display device dependent Information structure */
typedef struct {
	int width;
	int height;
	Display *disp;
	int scrn;
	Colormap colormap;
} DisplayInfo;

/* This struct holds the X-client side bits for a rendered image. */

typedef struct {
	Display *disp;		/* destination display */
	int scrn;		/* destination screen */
	int depth;		/* depth of drawable we want/have */
	Drawable drawable;	/* drawable to send image to */
	Pixel *index;		/* array of pixel values allocated */
	int no;			/* number of pixels in the array */
	boolean rootimage;	/* True if is a root image - eg, retain colors */
	Pixel foreground;	/* foreground and background pixels for mono images */
	Pixel background;
	Colormap cmap;		/* colormap used for image */
	GC gc;			/* cached gc for sending image */
	XImage *ximage;		/* ximage structure */
	XShmSegmentInfo shm;	/* valid if shm.shmid >= 0 */
} XImageInfo;

/* ddxli.c */
char *xliDisplayName(char *name);
int xliParseXColor(DisplayInfo *dinfo, char *spec, XColor *xlicolor);
int xliDefaultVisual(void);
void xliGammaCorrectXColor(XColor *xlicolor, double gamma);
