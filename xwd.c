/* xwd.c:
 *
 * XWD file reader.  unfortunately the bozo who thought up this format didn't
 * define anything at all that we can use as an identifier or even to tell
 * what kind of machine dumped the format.  what this does is read the
 * header and look at several fields to decide if this *might* be an XWD
 * file and if it is what byte order machine wrote it.
 *
 * jim frost 07.24.90
 *
 * Copyright 1990 Jim Frost.  See included file "copyright.h" for complete
 * copyright information.
 */

#include "copyright.h"
#include "xli.h"
#include "imagetypes.h"
#include "xwd.h"

/* this reads the header and does the magic to determine if it is indeed
 * an XWD file.
 */

static int isXWD(char *name, ZFILE * zf, XWDHeader * header, int verbose)
{
	GenericXWDHeader gh;
	int a;
	int type = NOT_XWD;

	if (zread(zf, (byte *) & gh, sizeofGenericXWDHeader) != sizeofGenericXWDHeader)
		return (0);

	/* first try -- see if XWD version number matches in either MSB or LSB order
	 */

	if (memToVal(gh.file_version, 4) != XWD_VERSION) {
		if (memToValLSB(gh.file_version, 4) != XWD_VERSION)
			return (0);
		else
			type = XWD_LSB;
	} else
		type = XWD_MSB;

	/* convert fields to fill out header.  things we don't care about
	 * are commented out.
	 */

	if (type == XWD_MSB) {
		header->header_size = memToVal(gh.header_size, 4);
		header->file_version = memToVal(gh.file_version, 4);
		header->pixmap_format = memToVal(gh.pixmap_format, 4);
		header->pixmap_depth = memToVal(gh.pixmap_depth, 4);
		header->pixmap_width = memToVal(gh.pixmap_width, 4);
		header->pixmap_height = memToVal(gh.pixmap_height, 4);
		header->xoffset = memToVal(gh.xoffset, 4);
		header->byte_order = memToVal(gh.byte_order, 4);
		header->bitmap_unit = memToVal(gh.bitmap_unit, 4);
		header->bitmap_bit_order = memToVal(gh.bitmap_bit_order, 4);
		header->bitmap_pad = memToVal(gh.bitmap_pad, 4);
		header->bits_per_pixel = memToVal(gh.bits_per_pixel, 4);
		header->bytes_per_line = memToVal(gh.bytes_per_line, 4);
		header->visual_class = memToVal(gh.visual_class, 4);
		/*header->red_mask= memToVal(gh.red_mask, 4); */
		/*header->green_mask= memToVal(gh.green_mask, 4); */
		/*header->blue_mask= memToVal(gh.blue_mask, 4); */
		/*header->bits_per_rgb= memToVal(gh.bits_per_rgb, 4); */
		header->colormap_entries = memToVal(gh.colormap_entries, 4);
		header->ncolors = memToVal(gh.ncolors, 4);
		/*header->window_width= memToVal(gh.window_width, 4); */
		/*header->window_height= memToVal(gh.window_height, 4); */
		/*header->window_x= memToVal(gh.window_x, 4); */
		/*header->window_y= memToVal(gh.window_y, 4); */
		/*header->window_bdrwidth= memToVal(gh.window_bdrwidth, 4); */
	} else {
		header->header_size = memToValLSB(gh.header_size, 4);
		header->file_version = memToValLSB(gh.file_version, 4);
		header->pixmap_format = memToValLSB(gh.pixmap_format, 4);
		header->pixmap_depth = memToValLSB(gh.pixmap_depth, 4);
		header->pixmap_width = memToValLSB(gh.pixmap_width, 4);
		header->pixmap_height = memToValLSB(gh.pixmap_height, 4);
		header->xoffset = memToValLSB(gh.xoffset, 4);
		header->byte_order = memToValLSB(gh.byte_order, 4);
		header->bitmap_unit = memToValLSB(gh.bitmap_unit, 4);
		header->bitmap_bit_order = memToValLSB(gh.bitmap_bit_order, 4);
		header->bitmap_pad = memToValLSB(gh.bitmap_pad, 4);
		header->bits_per_pixel = memToValLSB(gh.bits_per_pixel, 4);
		header->bytes_per_line = memToValLSB(gh.bytes_per_line, 4);
		header->visual_class = memToValLSB(gh.visual_class, 4);
		/*header->red_mask= memToValLSB(gh.red_mask, 4); */
		/*header->green_mask= memToValLSB(gh.green_mask, 4); */
		/*header->blue_mask= memToValLSB(gh.blue_mask, 4); */
		/*header->bits_per_rgb= memToValLSB(gh.bits_per_rgb, 4); */
		header->colormap_entries = memToValLSB(gh.colormap_entries, 4);
		header->ncolors = memToValLSB(gh.ncolors, 4);
		/*header->window_width= memToValLSB(gh.window_width, 4); */
		/*header->window_height= memToValLSB(gh.window_height, 4); */
		/*header->window_x= memToValLSB(gh.window_x, 4); */
		/*header->window_y= memToValLSB(gh.window_y, 4); */
		/*header->window_bdrwidth= memToValLSB(gh.window_bdrwidth, 4); */
	}

	/* if header size isn't either 100 or 104 bytes, this isn't an XWD file
	 */

	if (header->header_size < sizeofGenericXWDHeader)
		return (0);

	for (a = header->header_size - sizeofGenericXWDHeader; a; a--)
		zgetc(zf);

	/* look at a variety of the XImage fields to see if they are sane.  if
	 * they are, this passes our tests.
	 */

	switch (header->pixmap_format) {
	case XYBitmap:
	case XYPixmap:
	case ZPixmap:
		break;
	default:
		return (0);
	}

	switch (header->visual_class) {
	case StaticGray:
	case GrayScale:
	case StaticColor:
	case PseudoColor:

		/* the following are unsupported but recognized
		 */

	case TrueColor:
	case DirectColor:
		break;
	default:
		return (0);
	}

	if (verbose) {
		printf("%s is a %dx%d XWD image in ",
		       name, header->pixmap_width, header->pixmap_height);
		switch (header->pixmap_format) {
		case XYBitmap:
			printf("XYBitmap");
			break;
		case XYPixmap:
			printf("%d bit XYPixmap", header->pixmap_depth);
			break;
		case ZPixmap:
			printf("%d bit ZPixmap", header->pixmap_depth);
			break;
		}
		if (type == XWD_LSB)
			printf(" LSB header, ");
		else
			printf(" MSB header, ");
		if (header->pixmap_format != ZPixmap ||
				header->pixmap_depth == 1) {
			if (header->bitmap_bit_order == LSBFirst) {
				printf(" LSB bit, %d units,",
					header->bitmap_unit);
			} else {
				printf(" MSB bit, %d units,",
					header->bitmap_unit);
			}
		}
		if (header->byte_order == LSBFirst)
			printf(" LSB byte order");
		else
			printf(" MSB byte order");
		printf("\n");
	}
	/* if it got this far, we're pretty damned certain we've got the right
	 * file type and know what order it's in.
	 */

	return (type);
}

int xwdIdent(char *fullname, char *name)
{
	ZFILE *zf;
	XWDHeader header;
	int ret;

	if (!(zf = zopen(fullname)))
		return (0);
	ret = isXWD(name, zf, &header, 1);
	zclose(zf);
	return (ret);
}

static Image *loadXYBitmap(char *name, ZFILE * zf, XWDHeader header, int type)
{
	Image *image;
	int dlinelen;		/* length of scan line in data file */
	int ilinelen;		/* length of line within image structure */
	int unit;		/* # of bytes in a bitmap unit */
	int xoffset;		/* xoffset within line */
	int xunits;		/* # of units across the whole scan line */
	int trailer;		/* # of bytes in last bitmap unit on a line */
	int shift;		/* # of bits to shift last byte set */
	int x, y;		/* horizontal and vertical counters */
	byte *line;		/* input scan line */
	byte *dptr, *iptr;	/* image data pointers */

	image = newBitImage(header.pixmap_width, header.pixmap_height);
	ilinelen = (header.pixmap_width / 8) + (header.pixmap_width % 8 ? 1 : 0);
	if (header.bitmap_unit > 7)	/* supposed to be 8, 16, or 32 but appears */
		unit = header.bitmap_unit / 8;	/* to often be the byte count.  this will */
	else			/* accept either. */
		unit = header.bitmap_unit;
	xoffset = (header.xoffset / (unit * 8)) * unit;
	if (header.bytes_per_line)
		dlinelen = header.bytes_per_line;
	else
		dlinelen = unit * header.pixmap_width;
	xunits = (header.pixmap_width / (unit * 8)) +
	    (header.pixmap_width % (unit * 8) ? 1 : 0);
	trailer = unit - ((xunits * unit) - ilinelen);
	xunits--;		/* we want to use one less than the actual # of units */
	shift = (unit - trailer) * 8;
	line = (byte *) lmalloc(dlinelen);

	for (y = 0; y < header.pixmap_height; y++) {
		if (zread(zf, (byte *) line, dlinelen) != dlinelen) {
			fprintf(stderr,
				"xwdLoad: %s -  Short read (returning partial image)\n", name);
			lfree(line);
			return (image);
		}
		dptr = line + xoffset;
		iptr = image->data + (y * ilinelen);

		if (header.bitmap_bit_order == LSBFirst)
			flipBits(line, dlinelen);

		/* The byte swap conditions are a little complex as three things cause a 
		 * need to swap. If the byte_order is wrong we need to swap. If the bit
		 * order is wrong we need to swap the bits across bytes. If the whole
		 * file is byte reversed we need to swap. These swaps may cancel out,
		 * hence the xor expression below.
		 */
		if ((header.byte_order == MSBFirst) ^ (header.bitmap_bit_order == MSBFirst)
		    ^ (type == XWD_MSB))
			for (x = 0; x < xunits; x++) {	/* Don't swap bytes */
				register unsigned long temp;
				temp = memToVal(dptr, unit);
				valToMem(temp, iptr, unit);
				dptr += unit;
				iptr += unit;
		} else
			for (x = 0; x < xunits; x++) {	/* swap bytes */
				register unsigned long temp;
				temp = memToValLSB(dptr, unit);
				valToMem(temp, iptr, unit);
				dptr += unit;
				iptr += unit;
			}

		/* take care of last unit on this line
		 */

		if ((header.byte_order == MSBFirst)
		    ^ (header.bitmap_bit_order == MSBFirst) ^ (type == XWD_MSB)) {
			register unsigned long temp;
			temp = memToVal(dptr, unit);
			valToMem(temp >> shift, iptr, trailer);
		} else {
			register unsigned long temp;
			temp = memToValLSB(dptr, unit);
			valToMem(temp >> shift, iptr, trailer);
		}
	}

	lfree(line);
	return (image);
}

/* this is a lot like the above function but OR's planes together to
 * build the destination.  1-bit images are handled by XYBitmap.
 */

static Image *loadXYPixmap(char *name, ZFILE * zf, XWDHeader header, int type)
{
	Image *image;
	int plane;
	int dlinelen;		/* length of scan line in data file */
	int ilinelen;		/* length of line within image structure */
	int unit;		/* # of bytes in a bitmap unit */
	int unitbits;		/* # of bits in a bitmap unit */
	int unitmask;		/* mask for current bit within current unit */
	int xoffset;		/* xoffset within data */
	int xunits;		/* # of units across the whole scan line */
	int x, x2, y;		/* horizontal and vertical counters */
	int index;		/* index within image scan line */
	byte *line;		/* input scan line */
	byte *dptr, *iptr;	/* image data pointers */
	unsigned long pixvals;	/* bits for pixels in this unit */
	unsigned long mask;

	image = newRGBImage(header.pixmap_width, header.pixmap_height,
			    header.pixmap_depth);
	ilinelen = image->width * image->pixlen;
	if (header.bitmap_unit > 7)	/* supposed to be 8, 16, or 32 but appears */
		unit = header.bitmap_unit / 8;	/* to often be the byte count.  this will */
	else			/* accept either. */
		unit = header.bitmap_unit;
	unitbits = unit * 8;
	unitmask = 1 << (unitbits - 1);
	xoffset = (header.xoffset / unitbits) * unit;
	if (header.bytes_per_line)
		dlinelen = header.bytes_per_line;
	else
		dlinelen = unit * header.pixmap_width;
	xunits = (header.pixmap_width / (unit * 8)) +
	    (header.pixmap_width % (unit * 8) ? 1 : 0);
	line = (byte *) lmalloc(dlinelen);

	/* for each plane, load in the bitmap and or it into the image
	 */

	for (plane = header.pixmap_depth; plane > 0; plane--) {
		for (y = 0; y < header.pixmap_height; y++) {
			if (zread(zf, (byte *) line, dlinelen) != dlinelen) {
				fprintf(stderr,
					"xwdLoad: %s - Short read (returning partial image)\n", name);
				lfree(line);
				return (image);
			}
			dptr = line + xoffset;
			iptr = image->data + (y * ilinelen);
			index = 0;

			if (header.bitmap_bit_order == LSBFirst)
				flipBits(line, dlinelen);

			if ((header.byte_order == MSBFirst)
			    ^ (header.bitmap_bit_order == MSBFirst) ^ (type == XWD_MSB))
				for (x = 0; x < xunits; x++) {
					register unsigned long temp;
					pixvals = memToVal(dptr, unit);
					mask = unitmask;
					for (x2 = 0; x2 < unitbits; x2++) {
						if (pixvals & mask) {
							temp = memToVal(iptr + index, image->pixlen);
							valToMem(temp | (1 << plane), iptr + index, image->pixlen);
						}
						index += image->pixlen;
						if (index > ilinelen) {
							x = xunits;
							break;
						}
						if (!(mask >>= 1))
							mask = unitmask;
					}
					dptr += unit;
			} else
				for (x = 0; x < xunits; x++) {
					register unsigned long temp;
					pixvals = memToValLSB(dptr, unit);
					mask = unitmask;
					for (x2 = 0; x2 < unitbits; x2++) {
						if (pixvals & mask) {
							temp = memToVal(iptr + index, image->pixlen);
							valToMem(temp | (1 << plane), iptr + index, image->pixlen);
						}
						index += image->pixlen;
						if (index > ilinelen) {
							x = xunits;
							break;
						}
						if (!(mask >>= 1))
							mask = unitmask;
					}
					dptr += unit;
				}
		}
	}

	lfree(line);
	return (image);
}
/* this loads a ZPixmap format image.  note that this only supports depths
 * of 4, 8, 16, 24, or 32 bits as does Xlib.  You gotta 6-bit image,
 * you gotta problem.  1-bit images are handled by XYBitmap.
 */

static Image *loadZPixmap(char *name, ZFILE * zf, XWDHeader header, int type)
{
	Image *image;
	int dlinelen;		/* length of scan line in data file */
	int ilinelen;		/* length of scan line in image file */
	int depth;		/* depth rounded up to 8-bit value */
	int pixlen;		/* length of pixel in bytes */
	int x, y;		/* horizontal and vertical counters */
	byte *line;		/* input scan line */
	byte *dptr, *iptr;	/* image data pointers */
	unsigned long pixmask;	/* bit mask within pixel */
	unsigned long pixel;	/* pixel we're working on */

	image = newRGBImage(header.pixmap_width, header.pixmap_height,
			    header.pixmap_depth);

	/* for pixmaps that aren't simple depths, we round to a depth of 8.  this
	 * is what Xlib does, be it right nor not.
	 */

	if ((header.pixmap_depth != 4) && (header.pixmap_depth % 8))
		depth = header.pixmap_depth + 8 - (header.pixmap_depth % 8);
	else
		depth = header.pixmap_depth;

	pixmask = ((unsigned long) 0xffffffff) >> (32 - header.pixmap_depth);
	pixlen = image->pixlen;
	if (header.bytes_per_line)
		dlinelen = header.bytes_per_line;
	else
		dlinelen = depth * header.pixmap_width;
	ilinelen = image->width * image->pixlen;

	line = (byte *) lmalloc(dlinelen);

	for (y = 0; y < header.pixmap_height; y++) {
		if (zread(zf, (byte *) line, dlinelen) != dlinelen) {
			fprintf(stderr,
				"xwdLoad: %s - Short read (returning partial image)\n", name);
			lfree(line);
			return (image);
		}
		dptr = line;
		iptr = image->data + (y * ilinelen);

		switch (depth) {
		case 4:
			/* The protocol documents says we should take the order of */
			/* 4 bit pixels within a byte from the byte_order field - GWG */
			if (header.byte_order == LSBFirst) {	/* nybbles are reversed */
				for (x = 0; x < header.pixmap_width; x++) {
					pixel = memToVal(dptr, 1);
					valToMem(pixel & pixmask, iptr++, 1);
					if (++x < header.pixmap_width)
						valToMem((pixel >> 4) & pixmask, iptr++, 1);
				}
			} else {	/* MSB first */
				for (x = 0; x < header.pixmap_width; x++) {
					pixel = memToVal(dptr, 1);
					valToMem((pixel >> 4) & pixmask, iptr++, 1);
					if (++x < header.pixmap_width)
						valToMem(pixel & pixmask, iptr++, 1);
				}
			}
			break;
		case 8:
			for (x = 0; x < header.pixmap_width; x++) {
				pixel = ((unsigned long) *(dptr++)) & pixmask;	/* loader isn't needed */
				valToMem(pixel, iptr++, 1);
			}
			break;
		case 16:
			if ((header.byte_order == MSBFirst) ^ (type != XWD_MSB)) {
				for (x = 0; x < header.pixmap_width; x++) {
					pixel = memToVal(dptr, 2);
					valToMem(pixel & pixmask, iptr, 2);
					dptr += 2;
					iptr += 2;
				}
			} else {
				for (x = 0; x < header.pixmap_width; x++) {
					pixel = memToValLSB(dptr, 2);
					valToMem(pixel & pixmask, iptr, 2);
					dptr += 2;
					iptr += 2;
				}
			}
			break;
		case 24:
			if ((header.byte_order == MSBFirst) ^ (type != XWD_MSB)) {
				for (x = 0; x < header.pixmap_width; x++) {
					pixel = memToVal(dptr, 3);
					valToMem(pixel & pixmask, iptr, 3);
					dptr += 3;
					iptr += 3;
				}
			} else {
				for (x = 0; x < header.pixmap_width; x++) {
					pixel = memToValLSB(dptr, 3);
					valToMem(pixel & pixmask, iptr, 3);
					dptr += 3;
					iptr += 3;
				}
			}
			break;
		case 32:
			if ((header.byte_order == MSBFirst) ^ (type != XWD_MSB)) {
				for (x = 0; x < header.pixmap_width; x++) {
					pixel = memToVal(dptr, 4);
					valToMem(pixel & pixmask, iptr, 4);
					dptr += 4;
					iptr += 4;
				}
			} else {
				for (x = 0; x < header.pixmap_width; x++) {
					pixel = memToValLSB(dptr, 4);
					valToMem(pixel & pixmask, iptr, 4);
					dptr += 4;
					iptr += 4;
				}
			}
			break;
		default:
			fprintf(stderr,
				"xwdLoad: %s - ZPixmaps of depth %d are not supported (sorry).\n",
				name, header.pixmap_depth);
			lfree(line);
			freeImage(image);
			return (NULL);
		}
	}

	lfree(line);
	return (image);
}

Image *xwdLoad(char *fullname, ImageOptions * image_ops, boolean verbose)
{
	ZFILE *zf;
	char *name = image_ops->name;
	XWDHeader header;
	int cmaplen;
	XWDColor *cmap;
	byte *cmapptr;		/* Pointer into structure elements */
	Image *image;
	int a;
	int type;

	if (!(zf = zopen(fullname))) {
		perror("xwdLoad");
		return (NULL);
	}
	if (!(type = isXWD(name, zf, &header, verbose))) {
		zclose(zf);
		return (NULL);
	}
	znocache(zf);

	/* complain if we don't understand the visual
	 */

	switch (header.visual_class) {
	case StaticGray:
	case GrayScale:
	case StaticColor:
	case PseudoColor:
		break;
	case TrueColor:
	case DirectColor:
		fprintf(stderr,
			"xwdLoad: %d - Unsupported visual type, sorry\n",
			header.visual_class);
		zclose(zf);
		return (NULL);
	}

	if ((header.pixmap_width == 0) || (header.pixmap_height == 0)) {
		fprintf(stderr, "Zero-size image -- header might be corrupted.\n");
		zclose(zf);
		return (NULL);
	}
	/* read in colormap
	 */

	cmaplen = header.ncolors * sizeofXWDColor;
	cmap = (XWDColor *) lmalloc(cmaplen);
	if (zread(zf, (byte *) cmap, cmaplen) != cmaplen) {
		fprintf(stderr, "Short read in colormap!\n");
		lfree((byte *) cmap);
		zclose(zf);
		return (NULL);
	}
	/* any depth 1 image is basically a XYBitmap so we fake it here
	 */

	if (header.pixmap_depth == 1)
		header.pixmap_format = XYBitmap;

	/* we can't realistically support images of more than depth 16 with the
	 * RGB image format so this nukes them for the time being.
	 */

	if (header.pixmap_depth > 16) {
		fprintf(stderr,
			"%s: Sorry, cannot load images deeper than 16 bits (yet)\n",
			name);
		lfree((byte *) cmap);
		zclose(zf);
		return (NULL);
	}
	/* load the colormap.
	 */

	if (header.pixmap_format == XYBitmap) {
		image = loadXYBitmap(name, zf, header, type);
		image->rgb.used = 2;
		if (type == XWD_MSB)
			for (a = 0, cmapptr = (byte *) cmap;
			     a < 2;
			     a++, cmapptr += sizeofXWDColor) {
				image->rgb.red[a] = memToVal(cmapptr + 4, 2);
				image->rgb.green[a] = memToVal(cmapptr + 6, 2);
				image->rgb.blue[a] = memToVal(cmapptr + 8, 2);
		} else
			for (a = 0, cmapptr = (byte *) cmap;
			     a < 2;
			     a++, cmapptr += sizeofXWDColor) {
				image->rgb.red[a] = memToValLSB(cmapptr + 4, 2);
				image->rgb.green[a] = memToValLSB(cmapptr + 6, 2);
				image->rgb.blue[a] = memToValLSB(cmapptr + 8, 2);
			}
	} else {		/* color format */
		if (header.pixmap_format == XYPixmap)
			image = loadXYPixmap(name, zf, header, type);
		else		/*  ZPixmap */
			image = loadZPixmap(name, zf, header, type);
		if (image == NULL) {
			zclose(zf);
			return NULL;
		}
		image->rgb.used = header.ncolors;
		if (type == XWD_MSB)
			for (a = 0, cmapptr = (byte *) cmap;
			     a < header.ncolors;
			     a++, cmapptr += sizeofXWDColor) {
				int pixelval = memToVal(cmapptr, 4);
				image->rgb.red[pixelval] = memToVal(cmapptr + 4, 2);
				image->rgb.green[pixelval] = memToVal(cmapptr + 6, 2);
				image->rgb.blue[pixelval] = memToVal(cmapptr + 8, 2);
		} else
			for (a = 0, cmapptr = (byte *) cmap;
			     a < header.ncolors;
			     a++, cmapptr += sizeofXWDColor) {
				int pixelval = memToValLSB(cmapptr, 4);
				image->rgb.red[pixelval] = memToValLSB(cmapptr + 4, 2);
				image->rgb.green[pixelval] = memToValLSB(cmapptr + 6, 2);
				image->rgb.blue[pixelval] = memToValLSB(cmapptr + 8, 2);
			}
	}
	read_trail_opt(image_ops, zf, image, verbose);
	zclose(zf);
	image->title = dupString(name);
	lfree((byte *) cmap);
	return (image);
}
