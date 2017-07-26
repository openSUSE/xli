/* #ident	"@(#)x11:contrib/clients/xloadimage/img.h 1.5 93/07/23 Labtam" */
/*
** img.h - structures/definitions for GEM Bit Image format conversion
**
**	Tim Northrup <tim@BRS.Com>
**
**	Version 0.1 --  4/25/91 -- Initial cut
**
**  Copyright (C) 1991 Tim Northrup
**	(see file "tgncpyrght.h" for complete copyright information)
*/

#ifndef GEM_H
#define GEM_H

/*
**  Standard IMG Header Structure (8 words long)
**
**  May be followed by implementation-dependent information, so the
**	value of hlen should be checked on input.
*/

typedef struct {
	short	vers,			/* image version number */
		hlen,			/* header length (in words) */
		colors,			/* number of color planes (1=mono) */
		patlen,			/* pattern length (for encoding) */
		pixw, pixh,		/* pixel dimensions (in microns) */
		llen, lines;		/* pixels/line and number of lines */
	} IMG_Header;

/*
**	Header record values used when creating an IMG file
*/

#define DEF_VERSION	1		/* default version number (on output) */
#define DEF_HLEN	8		/* always standard header */
#define DEF_COLORS	1		/* always B/W pics */
#define DEF_PATLEN	1		/* easiest pattern size */
#define DEF_PIXW	85		/* just a guess on this one */
#define DEF_PIXH	85		/* assumed 1:1 aspect ratio */

/*
**	Program limits and other constants
*/

#define MAX_SCANLINE	1024		/* max bytes for 1 scanline */

#define MAX_SLREPEAT	255		/* max repititions of scanline */
#define MAX_PATREPEAT	255		/* max repititions of pattern */
#define MAX_BYTEREPEAT	127		/* max repititions of byte value */
#define MAX_LITERAL	255		/* max literal length allowed */

#define BYTE_FLAG	0x00		/* first byte 0 the ... */
#define  BYTE_SLREPEAT     0x00		   /* second 0 means scanline repeat */
#define   BYTE_SLFLAG	      0xFF	   /* always followed by FF (?) */
#define BYTE_LITERAL	0x80		/* char 80h - flag literal string */

#define BYTE_BLACK	0xFF		/* byte is all 1's */
#define BIT_BLACK	1		/* single black bit */
#define BYTE_WHITE	0x00		/* byte is all 0's */
#define BIT_WHITE	0		/* single white bit */

#define RUN_BLACK	0x80		/* bit flag for run of all 0's or 1's */
#define RUN_WHITE	0x00

#define RUN_LENGTH(X)   ((X) & 0x7F)
#define RUN_COLOR(X)	((((X) & RUN_BLACK) == RUN_BLACK)?BIT_BLACK:BIT_WHITE)
#define PIXEL_COLOR(X)	((((X) & 0x01) == 1)?BIT_BLACK:BIT_WHITE)

#endif /* GEM_H */
