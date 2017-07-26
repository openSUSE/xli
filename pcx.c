/*
** pcx.c - load a ZSoft PC Paintbrush (PCX) file for use inside xli
**
**	Tim Northrup <tim@BRS.Com>
**	Adapted from code by Jef Poskanzer (see Copyright below).
**
**	Version 0.1 --  4/25/91 -- Initial cut
**     Version 0.2 -- 2001-12-15 -- Support 8bit color images with palette.
**                                  (Alexandre Duret-Lutz <duret_g@epita.fr>)
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

/* Routine to load a PCX file */
static boolean PCX_LoadImage(ZFILE *zf, int in_bpr, int img_bpr,
                            Image *image, int rows, boolean readpal);


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
        img_bpr = (xmax * pcxhd[3] + 7)/8; /* Recompute the number of bytes
                                             per lines.  */

	/* double check image */
	if (xmax < 0 || ymax < 0 || in_bpr < img_bpr) {
		zclose(zf);
		return((Image *)NULL);
	}

	if (verbose)
		printf("%s is a %dx%d PC Paintbrush image\n",name,xmax,ymax);

        /* Only monochorme or 8bits paletized images are supported.  */
        if (pcxhd[65] > 1 || !(pcxhd[3] == 1 || pcxhd[3] == 8)) {
                fprintf(stderr,
                        "pcxLoad: %s - Unsuported PCX type\n",
                        name);

		zclose(zf);
		return((Image *)NULL);
	}

	znocache(zf);

        /* Allocate image. */
        if (pcxhd[3] == 1)
          image = newBitImage(xmax, ymax);
        else
          image = newRGBImage(xmax, ymax, pcxhd[3]);
	image->title = dupString(name);

	/* Read compressed bitmap. */
	if (!PCX_LoadImage(zf, in_bpr, img_bpr, image, ymax, pcxhd[3] == 8)) {
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

static boolean PCX_LoadImage (ZFILE *zf, int in_bpr, int img_bpr,
                             Image *image, int rows, boolean readpal)
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
        /* For binary image we need to reverse all bits.  */
        int xor_mask = (image->type == IBITMAP) ? 0xff : 0;

	ptr = &(image->data[0]);

	while (row < rows && (b = zgetc(zf)) != EOF) {

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

                b ^= xor_mask;

		for ( i = 0; i < cnt; i++ ) {
			if ( row >= rows ) {
				break;
				}
			if (bytes_this_row < img_bpr)
				*ptr++ = (unsigned char) b;
			if (++bytes_this_row == in_bpr) {
				/* start of a new line */
				row++;
				bytes_this_row = 0;
				}
			}
		}
       /* Read a palette if needed.  */
       if (readpal) {
               /* The palette is separated from the pixels data by dummy
                  byte equal to 12.  */
               if ((b = zgetc(zf)) == EOF || b != 12)
                       return FALSE;

               for (cnt = 0; cnt < 256; ++cnt) {
                       int r, g, b;
                       if ((r = zgetc(zf)) == EOF
                           || (g = zgetc(zf)) == EOF
                           || (b = zgetc(zf)) == EOF)
                               return FALSE;
                       image->rgb.red[cnt] = r << 8;
                       image->rgb.green[cnt] = g << 8;
                       image->rgb.blue[cnt] = b << 8;
               }
               image->rgb.used = 256;
       }

	return TRUE;
}
