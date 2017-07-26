/* xli.h:
 * jim frost 06.21.89
 *
 * Copyright 1989 Jim Frost.  See included file "copyright.h" for complete
 * copyright information.
 */

#if defined(SVR4) && !defined(SYSV)
#define SYSV			/* SYSV is out System V flag */
#endif

#include "copyright.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifndef VMS
#include <unistd.h>
#endif

#include "ddxli.h"
#include "image.h"
#include "options.h"

#ifndef INIT_MAXIMAGES
#define INIT_MAXIMAGES 1024
#endif

/* image name and option structure used when processing arguments */
typedef struct {
	char *name;		/* name of image */
	char *fullname;		/* full pathname for -delete */
	int loader_idx;		/* loader last used successfully with image */
	int atx, aty;		/* location to load image at */
	boolean ats;		/* TRUE if atx and aty have been set */
	unsigned int bright;	/* brightness multiplier */
	boolean center;		/* true if image is to be centered */
	int clipx, clipy;	/* Offset and area of image to be used */
	unsigned int clipw, cliph;
	char *border;		/* Border colour used in clipping */
	XColor bordercol;	/* X RGB of above */
	boolean colordither;
				/* true if color reduction is to dither image */
	unsigned int colors;	/* max # of colors to use for this image */
	int delay;		/* # of seconds delay before auto pic advance */
	unsigned int dither;	/* true if image is to be dithered */
	boolean expand;		/* true if image should be forced to
				 * TrueColor depth
				 */
	float gamma;		/* display gamma */
	boolean gray;		/* true if image is to be grayed */
	boolean merge;		/* true if we should merge onto previous */
	boolean normalize;	/* true if image is to be normalized */
	int rotate;		/* # degrees to rotate image */
	boolean smooth;		/* true if image is to be smoothed */
	char *title;		/* Override title on image */
	unsigned int xzoom, yzoom;
				/* zoom percentages */
	boolean zoom_auto;	/* automatically zoom to fit on screen */
	char *fg, *bg;		/* foreground/background colors if mono image */
	boolean done_to;	/* TRUE if we have already looked for trailing
				 * options
				 */
	char **to_argv;		/* If non-null, text of trailing options,
				 * parsed as for argv
				 */
	int xpmkeyc;		/* Overriding color context key value for xpm
				 * pixmaps */
	int iscale;		/* image-dependent scaling factor */
	boolean iscale_auto;	/* automatically iscale to fit on screen */
} ImageOptions;

/* globals and global options
 */

typedef struct {
	DisplayInfo dinfo;	/* device dependent display information */
	char *argv0;		/* name of this programs */
	char *lastfunc;		/* name of last function called (used in error
				 * reps) */
	int _Xdebug;		/* dump on X error flag */
	int _DumpCore;		/* dump on signal flag */
	boolean verbose;
	float display_gamma;
	char *dname;
	boolean do_fork;
	boolean forall;
	boolean fillscreen;
	boolean fit;
	boolean fullscreen;
	char *go_to;		/* label to goto */
	boolean identify;
	boolean install;
	boolean onroot;
	boolean private_cmap;
	boolean set_default;
	boolean use_pixmap;
	char *user_geometry;	/* -geometry passed by user */
	int visual_class;	/* user-defined visual class */
	unsigned int dest_window;
				/* window id to put image onto */
	boolean delete;		/* enable deleting current image with 'x' */
	boolean focus;		/* take keyboard focus when viewing in window */
} GlobalsRec;

/* Global declarations */

extern GlobalsRec globals;

#define CURRFUNC(aa) (globals.lastfunc = (aa))

/* Gamma correction stuff */

/* the default target display gamma. This can be overridden on the
 * command line or by settin an environment variable.
 */
#define DEFAULT_DISPLAY_GAMMA 2.2

/* the default IRGB image gamma. This can be overridden on the
 * command line.
 */
#define DEFAULT_IRGB_GAMMA 2.2

/* Compare gammas for equality */
#define GAMMA_NOT_EQUAL(g1,g2)   ((g1) > ((g2) + 0.00001) || (g1) < ((g2) - 0.00001))

/* Cached/uudecoded/uncompressed file I/O structures. */

struct cache {
	byte *end;
	byte buf[BUFSIZ];
	struct cache *next;
	boolean eof;
};

#define UULEN 128		/* uudecode buffer length */
#define UUBODY 1		/* uudecode state - reading body of file */
#define UUSKIP 2		/* uudecode state - skipping garbage */
#define UUEOF 3			/* uudecode stat - found eof */

typedef struct {
	unsigned int type;	/* ZIO file type */
	boolean nocache;	/* TRUE if caching has been disabled */
	FILE *stream;		/* file input stream */
	char *filename;		/* filename */
	struct cache *data;	/* data cache */
	struct cache *dataptr;	/* ptr to current cache block */
	byte *bufptr;		/* ptr within current cache block */
	byte *endptr;		/* ptr to end of current cache block */
	byte *auxb;		/* non NULL if auxiliary buffer in use */
	byte *oldbufptr;	/* save bufptr here when aux buffer is in use */
	byte *oldendptr;	/* save endptr here when aux buffer is in use */
	boolean eof;		/* TRUE if we are at encountered EOF */
	byte buf[BUFSIZ];	/* getc buffer when un-cached */
	boolean uudecode;	/* TRUE if being uudecoded */
	byte uubuf[UULEN];	/* uu decode buffer */
	byte *uunext, *uuend;
	int uustate;		/* state of uu decoder */
} ZFILE;

#define ZSTANDARD 0		/* standard file */
#define ZPIPE     1		/* file is a pipe (ie uncompress) */
#define ZSTDIN    2		/* file is stdin */

/*
   C library functions that may not be decalared elsewehere
 */

int atoi(const char *);
long atol(const char *);
double atof(const char *);
char *getenv(const char *);

/* function declarations */

/*
 * Note on image processing functions :-
 *
 * The assumption is always that an image processing function that returns
 * an image may return a new image or one of the input images, and that
 * input images are preserved.
 *
 * This means that within an image processing function the following
 * technique of managinging intermediate images is recomended:
 *
 * Image *func(isrc)
 * Image *isrc;
 *   {
 *   Image *src = isrc, *dst, *tmp;
 *   .
 *   .
 *   tmp = other_func(src);
 *   if (src != tmp && src != isrc)
 *     freeImage(src);
 *   src = tmp;
 *   .
 *   .
 *   dst = process(src);
 *   .
 *   .
 *   if (src != isrc)
 *     freeImage(src);
 *   return (dst);
 *
 * This may seem redundant in places, but allows changes to be made
 * without looking at the overal image structure usage.
 *
 */

/* imagetypes.c */
void supportedImageTypes(void);

/* misc.c */
char *tail(char *path);
void memoryExhausted(void);
void internalError(int sig);
void version(void);
void usage(char *name);
Image *processImage(DisplayInfo *dinfo, Image *iimage, ImageOptions *options);
int errorHandler(Display *disp, XErrorEvent *error);
extern short LEHexTable[];	/* Little Endian conversion value */
extern short BEHexTable[];	/* Big Endian conversion value */
#define HEXSTART_BAD -1		/* bitmap_faces useage */
#define HEXDELIM_BAD -2
#define HEXDELIM_IGNORE -3
#define HEXBAD   -4
void initLEHexTable(void);
void initBEHexTable(void);
/* ascii hex number to integer (string, length) */
int hstoi(unsigned char *s, int n);
char *xlistrstr(char *s1, char *s2);

/* path.c */
char *expandPath(char *p);
int findImage(char *name, char *fullname);
void listImages(void);
void loadPathsAndExts(void);
void showPath(void);

/* root.c */
void imageOnRoot(DisplayInfo *dinfo, Image *image, ImageOptions *options);

/* window.c */
void cleanUpWindow(DisplayInfo *dinfo);
char imageInWindow(DisplayInfo *dinfo, Image *image, ImageOptions *options,
	int argc, char **argv);

/* options.c */
int visualClassFromName(char *name);
char *nameOfVisualClass(int class);

/* clip.c */
Image *clip(Image *iimage, int clipx, int clipy, unsigned int clipw,
	unsigned int cliph, ImageOptions *imgopp);

/* bright.c */
void brighten(Image *image, unsigned int percent, unsigned int verbose);
void gray(Image *image, int verbose);
Image *normalize(Image *image, unsigned int verbose);
void gammacorrect(Image *image, float target_gam, unsigned int verbose);
extern int gammamap[256];
#define GAMMA16(color16) (gammamap[(color16)>>8]<<8)
#define GAMMA8(color8) (gammamap[(color8)])
#define GAMMA16to8(color16) (gammamap[(color16)>>8])
void defaultgamma(Image *image);

/* compress.c */
void compress_cmap(Image *image, unsigned int verbose);

/* dither.c */
Image *dither(Image *cimage, unsigned int verbose);

/* fill.c */
void fill(Image *image, unsigned int fx, unsigned int fy, unsigned int fw, unsigned int fh, Pixel pixval);

/* halftone.c */
Image *halftone(Image *cimage, unsigned int verbose);

/* imagetypes.c */
Image *loadImage(ImageOptions *image_ops, boolean verbose);
void identifyImage(char *name);

/* merge.c */
Image *merge(Image *idst, Image *isrc, int atx, int aty, ImageOptions *imgopp);

/* new.c */
extern unsigned long DepthToColorsTable[];
unsigned long colorsToDepth(long unsigned int ncolors);
char *dupString(char *s);
Image *newBitImage(unsigned int width, unsigned int height);
Image *newRGBImage(unsigned int width, unsigned int height, unsigned int depth);
Image *newTrueImage(unsigned int width, unsigned int height);
void freeImage(Image *image);
void freeImageData(Image *image);
void newRGBMapData(RGBMap *rgb, unsigned int size);
void resizeRGBMapData(RGBMap *rgb, unsigned int size);
void freeRGBMapData(RGBMap *rgb);
byte *lcalloc(unsigned int size);
byte *lmalloc(unsigned int size);
byte *lrealloc(byte *old, unsigned int size);
void lfree(byte *area);

/* options.c */
void help(char *option);
int doGeneralOption(OptionId opid, char **argv, ImageOptions *persist_ops,
	ImageOptions *image_ops);
int doLocalOption(OptionId opid, char **argv, boolean setpersist,
	ImageOptions *persist_ops, ImageOptions *image_ops);
void read_trail_opt(ImageOptions *image_ops, ZFILE *file, Image *image,
	boolean verbose);

/* rlelib.c */
void make_gamma(double gamma, int *gammamap);

/* reduce.c */
Image *reduce(Image *image, unsigned colors, int ditherf, float gamma,
	int verbose);
Image *expandtotrue(Image *image);
Image *expandbittoirgb(Image *image, int depth);
Image *expandirgbdepth(Image *image, int depth);

/* rotate.c */
Image *rotate(Image *iimage, int rotate, int verbose);

/* send.c */
void sendXImage(XImageInfo *xii, int src_x, int src_y, int dst_x, int dst_y,
	unsigned int w, unsigned int h);
XImageInfo *imageToXImage(Display *disp, int scrn, Visual *visual,
	unsigned int ddepth, Image *image, unsigned int private_cmap,
	unsigned int fit, ImageOptions *options);
Pixmap ximageToPixmap(Display *disp, Window parent, XImageInfo *xii);
void freeXImage(Image *image, XImageInfo *xii);

/* smooth.c */
Image *smooth(Image *isrc, int iterations, int verbose);

/* value.c */
void flipBits(byte *p, unsigned int len);

/* xpixmap.c */
int xpmoption(char *s);

/* zio.c */
ZFILE *zopen(char *name);
int zread(ZFILE *zf, byte *buf, int len);
int zeof(ZFILE *zf);
void zunread(ZFILE *zf, byte const *buf, int len);
char *zgets(char *buf, unsigned int size, ZFILE *zf);
boolean zrewind(ZFILE *zf);
void zclose(ZFILE *zf);
void znocache(ZFILE *zf);
void zforcecache(boolean);
void zreset(char *filename);
void zclearerr(ZFILE *zf);
int _zgetc(ZFILE *zf);
boolean _zopen(ZFILE *zf);
void _zreset(ZFILE *zf);
void _zclear(ZFILE *zf);
boolean _zreopen(ZFILE *zf);

#define zgetc(zf) (((zf)->bufptr < (zf)->endptr) ? *(zf)->bufptr++ : _zgetc(zf))

/* zoom.c */
Image *zoom(Image *oimage, unsigned int xzoom, unsigned int yzoom,
	boolean verbose, boolean changetitle);

/* ddxli.c */
boolean xliOpenDisplay(DisplayInfo *dinfo, char *name);
void xliCloseDisplay(DisplayInfo *dinfo);
void xliDefaultDispinfo(DisplayInfo *dinfo);
int xliDefaultDepth(void);
void tellAboutDisplay(DisplayInfo * dinfo);
