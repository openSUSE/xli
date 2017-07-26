/* #ident "@(#)x11:contrib/clients/xloadimage/bmp.h 1.2 94/07/29 Labtam" */
/*  */

/*
 * bmp.h - header file for Targa files
 */
 
#define C_WIN   1		/* Image class */
#define C_OS2   2

#define BI_RGB  0		/* Compression type */
#define BI_RLE8 1
#define BI_RLE4 2

#define BMP_FILEHEADER_LEN 14

#define WIN_INFOHEADER_LEN 40
#define OS2_INFOHEADER_LEN 12

/* Header structure definition. */
typedef struct {
	char *name;					/* stash pointer to name here too */
    int class;					/* Windows or OS/2 */

	unsigned long bfSize;		/* Size of file in bytes */
	unsigned int  bfxHotSpot;	/* Not used */
	unsigned int  bfyHotSpot;	/* Not used */
	unsigned long bfOffBits;	/* Offset of image bits from start of header */

	unsigned long biSize;		/* Size of info header in bytes */
	unsigned long biWidth;		/* Image width in pixels */
	unsigned long biHeight;		/* Image height in pixels */
	unsigned int  biPlanes;		/* Planes. Must == 1 */
	unsigned int  biBitCount;	/* Bits per pixels. Must be 1, 4, 8 or 24 */
	unsigned long biCompression;	/* Compression type */
	unsigned long biSizeImage;		/* Size of image in bytes */
	unsigned long biXPelsPerMeter;	/* X pixels per meter */
	unsigned long biYPelsPerMeter;	/* Y pixels per meter */
	unsigned long biClrUsed;		/* Number of colormap entries (0 == max) */
	unsigned long biClrImportant;	/* Number of important colors */
	} bmpHeader;

