/*
** pcx.c - load a ZSoft PC Paintbrush (PCX) file for use inside xli
**
**	Tim Northrup <tim@BRS.Com>
**	Adapted from code by Jef Poskanzer (see Copyright below).
**
**	Version 0.1 --  4/25/91 -- Initial cut
**
**  Copyright (c) 1991 Tim Northrup
**	(see file "tgncpyrght.h" for complete copyright information)
*/

/*
** Copyright (C) 1988 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
**
** This program (pcxtopbm) is based on the pcx2rf program by:
**   Mike Macgirvin
**   Stanford Relativity Gyro Program GP-B
**   Stanford, Calif. 94503
**   ARPA: mike@relgyro.stanford.edu
*/

#include "tgncpyrght.h"
#include "xli.h"
#include "imagetypes.h"

#define PCX_MAGIC 0x0a			/* first byte in a PCX image file */

static boolean PCX_LoadImage(ZFILE *zf, int in_bpr, int img_bpr, Image *image, int rows);		/* Routine to load a PCX file */


/*
**  pcxIdent
**
**	Identify passed file as a PC Paintbrush image or not
**
**	Returns 1 if file is a PCX file, 0 otherwise
*/

int pcxIdent(char *fullname, char *name)
{
	ZFILE *zf;
	int  ret;
	int xmin;
	int xmax;
	int ymin;
	int ymax;
	unsigned char pcxhd[128];

	ret = 0;

	if (! (zf = zopen(fullname))) {
		perror("pcxIdent");
		return(0);
	}

	/* Read .pcx header. */
	if (zread(zf,pcxhd,128) == 128) {
		if (pcxhd[0] == PCX_MAGIC) {
			/* Calculate raster header and swap bytes. */
			xmin = pcxhd[4] + ( 256 * pcxhd[5] );
			ymin = pcxhd[6] + ( 256 * pcxhd[7] );
			xmax = pcxhd[8] + ( 256 * pcxhd[9] );
			ymax = pcxhd[10] + ( 256 * pcxhd[11] );
			xmax = xmax - xmin + 1;
			ymax = ymax - ymin + 1;

			printf("%s is a %dx%d PC Paintbrush image\n",
				name,xmax,ymax);
			ret = 1;
			}
		}

	zclose(zf);
	return(ret);
}


/*
**  pcxLoad
**
**	Load PCX Paintbrush file into an Image structure.
**
**	Returns pointer to allocated struct if successful, NULL otherwise
*/

Image *pcxLoad (char *fullname, ImageOptions *image_ops, boolean verbose)
{
	ZFILE *zf;
	char *name = image_ops->name;
	unsigned char pcxhd[128];
	int xmin;
	int xmax;
	int ymin;
	int ymax;
	int in_bpr;		/* in input format bytes per row */
	int img_bpr;	/* xli image format bytes per row */
	register Image *image;

	/* Open input file. */
	if ( ! (zf = zopen(fullname))) {
		perror("pcxLoad");
		return((Image *)NULL);
	}

	/* Read .pcx header. */
	if (zread(zf,pcxhd,128) != 128) {
		zclose(zf);
		return((Image *)NULL);
		}

	if ((pcxhd[0] != PCX_MAGIC) || (pcxhd[1] > 5)) {
		zclose(zf);
		return((Image *)NULL);
		}

	/* Calculate raster header and swap bytes. */
	xmin = pcxhd[4] + ( 256 * pcxhd[5] );
	ymin = pcxhd[6] + ( 256 * pcxhd[7] );
	xmax = pcxhd[8] + ( 256 * pcxhd[9] );
	ymax = pcxhd[10] + ( 256 * pcxhd[11] );
	xmax = xmax - xmin + 1;
	ymax = ymax - ymin + 1;
	in_bpr = pcxhd[66] + ( 256 * pcxhd[67] );
	img_bpr = (xmax + 7)/8;		/* assumes monochrome image */

	/* double check image */
	if (xmax < 0 || ymax < 0 || in_bpr < img_bpr) {
		zclose(zf);
		return((Image *)NULL);
		}

	if (verbose)
		printf("%s is a %dx%d PC Paintbrush image\n",name,xmax,ymax);

	if (pcxhd[65] > 1) {
		fprintf(stderr,"pcxLoad: %s - Unable to handle Color PCX image\n",name);
		zclose(zf);
		return((Image *)NULL);
		}

	znocache(zf);

	/* Allocate pbm array. */
	image = newBitImage(xmax,ymax);
	image->title = dupString(name);

	/* Read compressed bitmap. */
	if (!PCX_LoadImage( zf, in_bpr, img_bpr, image, ymax )) {
		fprintf(stderr,"pcxLoad: %s - Short read of PCX file\n",name);
		zclose(zf);
		return(image);
    }

	read_trail_opt(image_ops,zf,image,verbose);
	zclose(zf);
	return(image);
}


/*
**  PCX_LoadImage
**
**	Load PC Paintbrush file into the passed Image structure.
**
**	Returns FALSE if there was a short read.
*/

static boolean PCX_LoadImage (ZFILE *zf, int in_bpr, int img_bpr, Image *image, int rows)
{
	/* Goes like this: Read a byte.  If the two high bits are set,
	** then the low 6 bits contain a repeat count, and the byte to
	** repeat is the next byte in the file.  If the two high bits are
	** not set, then this is the byte to write.
	*/

	register unsigned char *ptr;
	int row = 0;
	int bytes_this_row = 0;
	int b, i, cnt;

	ptr = &(image->data[0]);

	while ((b = zgetc(zf)) != EOF) {

		if ((b & 0xC0) == 0xC0) {
			/* have a repetition count -- mask flag bits */
			cnt = b & 0x3F;
			b = zgetc(zf);
			if (b == EOF)
				return FALSE;
			}
		else {
			cnt = 1;		/* no repeating this one */
			}

		for ( i = 0; i < cnt; i++ ) {
			if ( row >= rows ) {
				return TRUE;
				}
			if (bytes_this_row < img_bpr)
				*ptr++ = (unsigned char) (255 - b);
			if (++bytes_this_row == in_bpr) {
				/* start of a new line */
				row++;
				bytes_this_row = 0;
				}
			}
		}

	return TRUE;
}
