/* #ident	"@(#)x11:contrib/clients/xloadimage/img.c 1.6 93/07/23 Labtam" */
/*
** img.c - load a GEM Bit Image file for use inside xli
**
**	Tim Northrup <tim@BRS.Com>
**
**	Version 0.1 --  4/25/91 -- Initial cut
**
**  Copyright (C) 1991 Tim Northrup
**	(see file "tgncpyrght.h" for complete copyright information)
*/

#include "tgncpyrght.h"
#include "xli.h"
#include "imagetypes.h"
#include "img.h"

static int Debug = 0;				/* Set to 1 for info */

static unsigned char *BitRow;			/* working row of bits */
static int RowRepeat;				/* number of repititons */
static int RowCount;				/* current row */
static int ColCount;				/* current column */

static int IMG_ReadHeader(ZFILE *zf, IMG_Header *h);			/* Read file header */
static void IMG_WriteByte(unsigned char c, register int cols, unsigned int bpl);			/* Write output byte */

/*
**  imgIdent
**
**	Identify passed file as a GEM Bit Image or not
**
**	Returns 1 if file is a GEM Image, 0 otherwise
*/

int imgIdent (char *fullname, char *name)
{
	ZFILE *zf;				/* Input file */
	IMG_Header header;			/* GEM Image header info */
	int ret = 0;				/* Return value */

	if ( ! (zf = zopen(fullname)))
		return(0);

	if (IMG_ReadHeader(zf,&header) == 0) {
		printf("%s is a %dx%d GEM Bit Image\n", fullname,
		       header.llen, header.lines);
		ret = 1;
	      }

	zclose(zf);
	return(ret);
}

/*
**  imgLoad
**
**	Load GEM Image file into an Image structure.
**
**	Returns pointer to allocated struct if successful, NULL otherwise
*/

Image *imgLoad (char *fullname, ImageOptions *image_ops, boolean verbose)
{
	char *name = image_ops->name;
	register int i;				/* Random index */
	ZFILE *zf;				/* Input file */
	short creps;				/* Repetition counter */
	short ictr;				/* Secondary index counter */
	unsigned int bpl;			/* Bytes per scanline */
	Image *image;				/* Allocated image struct */
	IMG_Header header;			/* GEM Image header */
	unsigned char inbuf[MAX_LITERAL], ichr;	/* Input buffer/char */

	if ( ! (zf = zopen(fullname))) {
		perror("imgLoad");
		return((Image *)NULL);
	}

	if ((i = IMG_ReadHeader(zf,&header)) != 0) {
		zclose(zf);
		switch (i) {
		case 1: /* short read in header */
		case 2: /* not a GEM image */
		  break;
		case 3:
		  fprintf(stderr,"imgLoad: %s - Color images are not supported (yet)\n",name);
		  break;
		case 4:
		  fprintf(stderr,"imgLoad: %s - image is too large\n",name);
		  break;
		default:
		  fprintf(stderr,"imgLoad: %s - Unsupported image pattern length\n",name);
		  break;
		}
		return((Image *)NULL);
	      }

	if (verbose)
	    printf("%s is a %dx%d GEM Image\n",name,header.llen,header.lines);
	znocache(zf);

	image = newBitImage(header.llen,header.lines);

	image->title = dupString(name);
	BitRow = &(image->data[0]);

	bpl = header.llen / 8;				/* bytes per line */
	ColCount = 0;					/* starting column */
	RowCount = 0;					/* starting row */
	RowRepeat = 1;					/* default repeat */

	while (RowCount < header.lines) {

		/* While the last line has not been reached ... */

		i = zgetc(zf);
		if (i == EOF) {
			fprintf(stderr,"imgLoad: %s - premature EOF\n",name);
			break;
			}

		if (i == BYTE_FLAG) {		/* scanline */
			i = zgetc(zf);
			if (i == EOF) {
				fprintf(stderr,"imgLoad: %s - premature EOF\n",name);
				break;
				}
			if (i == BYTE_SLREPEAT) {	/* repeat scanline */
				if (ColCount != 0) {
				    printf("GEM Image Failure: corrupted\n");
					fprintf(stderr,"imgLoad: %s - premature EOF\n",name);
				    break;
				    }
				i = zgetc(zf);		/* SLFLAG byte (0xFF) */
				if (i != BYTE_SLFLAG) {
					fprintf(stderr,"imgLoad: %s - corrupted\n",name);
				    break;
				    }
				RowRepeat = zgetc(zf);   /* repeat count */
				}
			else {			/* repeat pattern_len */
				creps = i;
				zread(zf,inbuf,header.patlen);
				for (i = 0 ; i < creps ; i++)
				    for (ictr = 0 ; ictr < header.patlen ; ictr++)
					IMG_WriteByte(inbuf[ictr],header.llen,bpl);
				}
			}
		else if (i == BYTE_LITERAL) {	/* literal follows */
			creps = zgetc(zf);		/* literal length */
			zread(zf,inbuf,creps);		/* literal */
			for (ictr = 0 ; ictr < creps ; ictr++)
				IMG_WriteByte(inbuf[ictr],header.llen,bpl);
			}
		else {			/* monochrome mode bits */
			creps = RUN_LENGTH(i);
			ichr = (RUN_COLOR(i) == 1) ?
					BYTE_BLACK : BYTE_WHITE;
			for (i = 0 ; i < creps ; i++)
				IMG_WriteByte(ichr,header.llen,bpl);
			}
		}

	read_trail_opt(image_ops,zf,image,verbose);
	zclose(zf);
	return(image);
}

/*
**  IMG_ReadHeader
**
**	Read in IMG file header information, skip extras.
**
**	Returns 0 if successful
**		1 if not enough file for header record
**		2 if header data is nonsensical/invalid
**		3 if image is color (unsupported)
**		4 if image is too wide ( > 8K bits!!)
**		5 if pattern length is too big ( > 255)
*/

static int IMG_ReadHeader (ZFILE *zf, IMG_Header *h)
{
	register int tlen;			/* total to read in */
	register int rlen;			/* read lengths */
	unsigned char junkbuffer[MAX_SCANLINE];	/* scrap buffer */

	tlen = zread(zf,junkbuffer,(DEF_HLEN * 2));
	if (tlen != (DEF_HLEN * 2))
		return(1);			/* not enough data */

	/* convert from big-endian to machine specific */

	h->vers   = junkbuffer[1] + (256 * junkbuffer[0]);
	h->hlen   = junkbuffer[3] + (256 * junkbuffer[2]);
	h->colors = junkbuffer[5] + (256 * junkbuffer[4]);
	h->patlen = junkbuffer[7] + (256 * junkbuffer[6]);
	h->pixw   = junkbuffer[9] + (256 * junkbuffer[8]);
	h->pixh   = junkbuffer[11] + (256 * junkbuffer[10]);
	h->llen   = junkbuffer[13] + (256 * junkbuffer[12]);
	h->lines  = junkbuffer[15] + (256 * junkbuffer[14]);

	/* sanity check; if fields don't look right, it's probably not a GEM
	 * image.
	 */

	if ((h->vers != DEF_VERSION) ||
	    (h->hlen < DEF_HLEN) ||
	    (h->colors < 0) || (h->colors > 256) ||
	    (h->pixw < 1) ||
	    (h->pixh < 1) ||
	    (h->llen < 1) || (h->llen > (MAX_SCANLINE * 8)) ||
	    (h->lines < 1) || (h->lines > 8192) ||
	    (h->patlen < 0) || (h->patlen > MAX_LITERAL))
	  return(2);

	if (Debug) {
		fprintf(stderr,"Header Information:\n");
		fprintf(stderr,"\tIMG Version:  %d\n",h->vers);
		fprintf(stderr,"\t Header Len:  %d\n",h->hlen);
		fprintf(stderr,"\t     Colors:  %d\n",h->colors);
		fprintf(stderr,"\tPattern Len:  %d\n",h->patlen);
		fprintf(stderr,"\t Pixel Size:  %d x %d (microns=1000th mm)\n",
				h->pixw,h->pixh);
		fprintf(stderr,"\t Image Size:  %d x %d (pixels)\n",
				h->llen,h->lines);
		}

	/* validity check on particular fields
	 */

	if (h->colors != 1)
		return(3);
	if (h->llen > (MAX_SCANLINE * 8))
		return(4);
	if (h->patlen > MAX_LITERAL)
		return(5);

	/* make life easier if not on an even boundary */
	if (h->llen % 8) {
		h->llen += (8 - (h->llen % 8));
		if (Debug)
			fprintf(stderr,"Image expanded to %d pixels wide\n",
				h->llen);
		}

	/* skip additional header information if present */
	if (h->hlen > DEF_HLEN) {
		tlen = ((h->hlen - DEF_HLEN) * 2);
		if (Debug)
			fprintf(stderr,"imgLoad: %d bytes of extra header skipped",tlen);
		for ( ; tlen > 0 ; tlen -= rlen) {
			rlen = (tlen > MAX_SCANLINE) ? MAX_SCANLINE : tlen;
			zread(zf,junkbuffer,rlen);
			}
		}

	return(0);
}



/*
**  IMG_WriteByte
**
**	Add byte to image; if end of scanline, may need to replicate it
**
**	Returns no value (void function)
*/

static void IMG_WriteByte (unsigned char c, register int cols, unsigned int bpl)
{
	register int i;
	register unsigned char *ptr;
	register unsigned char *ptr2;

	BitRow[ColCount] = c;

	if (++ColCount >= bpl) {			/* end of scanline */
		ptr2 = BitRow + bpl;
		RowCount++;			/* count one already out */
		while (--RowRepeat > 0) {
			for (ptr = BitRow, i = 0 ; i < bpl ; i++, ptr++)
				*ptr2++ = *ptr;
			RowCount++;
			}
		BitRow = ptr2;
		ColCount = 0;
		RowRepeat = 1;
		}

	return;
}
