/* #ident   "@(#)x11:contrib/clients/xloadimage/tga.h 1.1 93/07/28 Labtam" */
/*
 *tga.h - header file for Targa files
 */
 
#define TGA_HEADER_LEN 18

/* Header structure definition. */
typedef struct {
	char *name;					/* stash pointer to name here too */
	unsigned int IDLength;		/* length of Identifier String */
	unsigned int CoMapType;		/* 0 = no map */
	unsigned int ImgType;		/* image type (see below for values) */
	unsigned int Index;			/* index of first color map entry */
	unsigned int Length;		/* number of entries in color map */
	unsigned int CoSize;		/* size of color map entry (15,16,24,32) */
	int          X_org;			/* x origin of image */
	int          Y_org;			/* y origin of image */
	unsigned int Width;			/* width of image */
	unsigned int Height;		/* height of image */
	unsigned int PixelSize;		/* pixel size (8,16,24,32) */
	unsigned int AttBits;		/* 4 bits, number of attribute bits per pixel */
	unsigned int Rsrvd;			/* 1 bit, reserved */
	unsigned int OrgBit;		/* 1 bit, origin: 0=lower left, 1=upper left */
	unsigned int IntrLve;		/* 2 bits, interleaving flag */
	boolean      RLE;			/* Run length encoded */
	} tgaHeader;

/* Definitions for image types. */
#define TGA_Null 0			/* Not used */
#define TGA_Map 1
#define TGA_RGB 2
#define TGA_Mono 3
#define TGA_RLEMap 9
#define TGA_RLERGB 10
#define TGA_RLEMono 11
#define TGA_CompMap 32		/* Not used */
#define TGA_CompMap4 33		/* Not used */

#define TGA_IMAGE_TYPE(tt) \
	tt == 1 ? "Pseudo color " : \
	tt == 2 ? "True color " : \
	tt == 3 ? "Gray scale " : \
	tt == 9 ? "Run length encoded pseudo color " : \
	tt == 10 ? "Run length encoded true color " : \
	tt == 11 ? "Run length encoded gray scale " : \
	           "Unknown "

/* Definitions for interleave flag. */
#define TGA_IL_None 0
#define TGA_IL_Two 1
#define TGA_IL_Four 2

#define TGA_IL_TYPE(tt) \
	tt == 0 ? "" : \
	tt == 1 ? "Two way interleaved " : \
	tt == 2 ? "Four way interleaved " : \
	          "Unknown "






