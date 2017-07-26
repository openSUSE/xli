/*
 * Read a Microsoft/IBM BMP file.
 *
 * Created for xli by Graeme Gill,
 *
 * Based on tga.c, and guided by the Microsoft file format
 * description, and bmptoppm.c by David W. Sanderson.
 *
 */

#include "xli.h"
#include "imagetypes.h"
#include "bmp.h"

#define GULONG4(bp) ((unsigned long)(bp)[0] + 256 * (unsigned long)(bp)[1] \
	+ 65536 * (unsigned long)(bp)[2] + 16777216 * (unsigned long)(bp)[3])
#define GULONG2(bp) ((unsigned long)(bp)[0] + 256 * (unsigned long)(bp)[1])
#define GUINT2(bp) ((unsigned int)(bp)[0] + 256 * (unsigned int)(bp)[1])

/* Read the header of the file, and */
/* Return TRUE if this looks like a bmp file */
static boolean read_bmpHeader(ZFILE * zf, bmpHeader * hp, char *name)
{
	unsigned char buf[WIN_INFOHEADER_LEN];	/* largest we'll need */

	if (zread(zf, buf, BMP_FILEHEADER_LEN) != BMP_FILEHEADER_LEN)
		return FALSE;

	if (buf[0] != 'B' || buf[1] != 'M')
		return FALSE;	/* bad magic number */

	hp->bfSize = GULONG4(&buf[2]);
	hp->bfxHotSpot = GUINT2(&buf[6]);
	hp->bfyHotSpot = GUINT2(&buf[8]);
	hp->bfOffBits = GULONG4(&buf[10]);

	/* Read enough of the file info to figure the type out */
	if (zread(zf, buf, 4) != 4)
		return FALSE;

	hp->biSize = GULONG4(&buf[0]);
	if (hp->biSize == WIN_INFOHEADER_LEN)
		hp->class = C_WIN;
	else if (hp->biSize == OS2_INFOHEADER_LEN)
		hp->class = C_OS2;
	else
		return FALSE;

	if (hp->class == C_WIN) {
		/* read in the rest of the info header */
		if (zread(zf, buf + 4, WIN_INFOHEADER_LEN - 4)
				!= WIN_INFOHEADER_LEN - 4)
			return FALSE;

		hp->biWidth = GULONG4(&buf[4]);
		hp->biHeight = GULONG4(&buf[8]);
		hp->biPlanes = GUINT2(&buf[12]);;
		hp->biBitCount = GUINT2(&buf[14]);;
		hp->biCompression = GULONG4(&buf[16]);;
		hp->biSizeImage = GULONG4(&buf[20]);;
		hp->biXPelsPerMeter = GULONG4(&buf[24]);;
		hp->biYPelsPerMeter = GULONG4(&buf[28]);;
		hp->biClrUsed = GULONG4(&buf[32]);
		hp->biClrImportant = GULONG4(&buf[36]);
	} else {		/* C_OS2 */
		/* read in the rest of the info header */
		if (zread(zf, buf + 4, OS2_INFOHEADER_LEN - 4)
				!= OS2_INFOHEADER_LEN - 4)
			return FALSE;

		hp->biWidth = GULONG2(&buf[4]);
		hp->biHeight = GULONG2(&buf[6]);
		hp->biPlanes = GUINT2(&buf[8]);;
		hp->biBitCount = GUINT2(&buf[10]);;
		hp->biCompression = BI_RGB;
		hp->biSizeImage = 0;
		hp->biXPelsPerMeter = 0;
		hp->biYPelsPerMeter = 0;
		hp->biClrUsed = 0;
		hp->biClrImportant = 0;
	}

	/* Check for file corruption */

	if (hp->biBitCount != 1
	    && hp->biBitCount != 4
	    && hp->biBitCount != 8
	    && hp->biBitCount != 24) {
		fprintf(stderr, "bmpLoad: %s - Illegal image BitCount\n",
			hp->name);
		return FALSE;
	}
	if ((hp->biCompression != BI_RGB
	     && hp->biCompression != BI_RLE8
	     && hp->biCompression != BI_RLE4)
	    || (hp->biCompression == BI_RLE8 && hp->biBitCount != 8)
	    || (hp->biCompression == BI_RLE4 && hp->biBitCount != 4)) {
		fprintf(stderr,
			"bmpLoad: %s - Illegal image compression type\n",
			hp->name);
		return FALSE;
	}
	if (hp->biPlanes != 1) {
		fprintf(stderr, "bmpLoad: %s - Illegal image Planes value\n",
			hp->name);
		return FALSE;
	}
	/* Fix up a few things */
	if (hp->biBitCount < 24) {
		if (hp->biClrUsed == 0
		    || hp->biClrUsed > (1 << hp->biBitCount))
			hp->biClrUsed = (1 << hp->biBitCount);
	} else
		hp->biClrUsed = 0;

	hp->name = name;
	return TRUE;
}

/* Print a brief description of the image */
static void tell_about_bmp(bmpHeader * hp)
{
	printf("%s is a %lux%lu %d bit deep, %s%s BMP image\n", hp->name,
		hp->biWidth, hp->biHeight, hp->biBitCount,
		hp->biCompression == BI_RGB ? "" : "Run length compressed, ",
		hp->class == C_WIN ? "Windows" : "OS/2");
}

int bmpIdent(char *fullname, char *name)
{
	ZFILE *zf;
	bmpHeader hdr;

	if (!(zf = zopen(fullname))) {
		perror("bmpIdent");
		return (0);
	}
	if (!read_bmpHeader(zf, &hdr, name)) {
		zclose(zf);
		return 0;	/* Nope, not a BMP file */
	}
	tell_about_bmp(&hdr);
	zclose(zf);
	return 1;
}

Image *bmpLoad(char *fullname, ImageOptions * image_ops, boolean verbose)
{
	ZFILE *zf;
	bmpHeader hdr;
	Image *image;
	boolean data_bounds = FALSE;
	int skip;

	if (!(zf = zopen(fullname))) {
		perror("bmpIdent");
		return (0);
	}
	if (!read_bmpHeader(zf, &hdr, image_ops->name)) {
		zclose(zf);
		return NULL;	/* Nope, not a BMP file */
	}
	if (verbose)
		tell_about_bmp(&hdr);
	znocache(zf);

	/* Create the appropriate image and colormap */
	if (hdr.biBitCount < 24) {
		/* must be 1, 4 or 8 bit mapped type */
		int i, n = 3;
		byte buf[4];
		/* maximum number of colors */
		int used = (1 << hdr.biBitCount);

		if (hdr.biBitCount == 1)	/* bitmap */
			image = newBitImage(hdr.biWidth, hdr.biHeight);
		else
			image = newRGBImage(hdr.biWidth, hdr.biHeight,
				hdr.biBitCount);
		image->title = dupString(hdr.name);

		if (hdr.class == C_WIN)
			n++;
		for (i = 0; i < hdr.biClrUsed; i++) {
			if (zread(zf, buf, n) != n) {
				fprintf(stderr, "bmpLoad: %s - Short read within Colormap\n", hdr.name);
				freeImage(image);
				zclose(zf);
				return NULL;
			}
			image->rgb.red[i] = buf[2] << 8;
			image->rgb.green[i] = buf[1] << 8;
			image->rgb.blue[i] = buf[0] << 8;
		}

		/* init rest of colormap (if any) */
		for (; i < used; i++) {
			image->rgb.red[i] =
			    image->rgb.green[i] =
			    image->rgb.blue[i] = 0;
		}
		/* Don't know how many colors are actually used. */
		/* (believing the header caould cause a fault) */
		/* compress() will figure it out. */
		image->rgb.used = used;		/* so use max possible */
	} else {		/* else must be a true color image */
		image = newTrueImage(hdr.biWidth, hdr.biHeight);
		image->title = dupString(hdr.name);
	}

	/* Skip to offset specified in file header for image data */
	if (hdr.class == C_WIN)
		skip = hdr.bfOffBits - (BMP_FILEHEADER_LEN
			+ WIN_INFOHEADER_LEN + 4 * hdr.biClrUsed);
	else
		skip = hdr.bfOffBits - (BMP_FILEHEADER_LEN
			+ OS2_INFOHEADER_LEN + 3 * hdr.biClrUsed);

	while (skip > 0) {
		if (zgetc(zf) == EOF)
			goto data_short;
		skip--;
	}

	/* Read the pixel data */
	if (hdr.biBitCount == 1) {
		byte *data, pad[4];
		int illen, padlen, y;

		/* round bits up to byte */
		illen = (image->width + 7) / 8;
		/* extra bytes to word boundary */	
		padlen = (((image->width + 31) / 32) * 4) - illen;
		/* start at bottom */
		data = image->data + (image->height - 1) * illen;
		for (y = image->height; y > 0; y--, data -= illen) {
			/* BMP files are left bit == ms bit,
			 * so read straight in.
			 */
			if (zread(zf, data, illen) != illen
			    || zread(zf, pad, padlen) != padlen)
				goto data_short;
		}
	} else if (hdr.biBitCount == 4) {
		byte *data;
		int illen, x, y;

		illen = image->width;
		/* start at bottom */
		data = image->data + (image->height - 1) * illen;

		if (hdr.biCompression == BI_RLE4) {
			int d, e;
			bzero((char *) image->data,
				image->width * image->height);
			for (x = y = 0;;) {
				int i, f;
				if ((d = zgetc(zf)) == EOF)
					goto data_short;
				if (d != 0) {	/* run of pixels */
					x += d;
					if (x > image->width ||
							y > image->height) {
						/* don't run off buffer */
						data_bounds = TRUE;
						/* ignore this run */
						x -= d;	
						if ((e = zgetc(zf)) == EOF)
							goto data_short;
						continue;
					}
					if ((e = zgetc(zf)) == EOF)
						goto data_short;
					f = e & 0xf;
					e >>= 4;
					for (i = d / 2; i > 0; i--) {
						*(data++) = e;
						*(data++) = f;
					}
					if (d & 1)
						*(data++) = e;
					continue;
				}
				/* else code */
				if ((d = zgetc(zf)) == EOF)
					goto data_short;
				if (d == 0) {	/* end of line */
					data -= (x + illen);
					x = 0;
					y++;
					continue;
				}
				/* else */
				if (d == 1)	/* end of bitmap */
					break;
				/* else */
				if (d == 2) {	/* delta */
					if ((d = zgetc(zf)) == EOF ||
							(e = zgetc(zf)) == EOF)
						goto data_short;
					x += d;
					data += d;
					y += e;
					data -= (e * illen);
					continue;
				}
				/* else run of literals */
				x += d;
				if (x > image->width || y > image->height) {
					int btr;
					/* don't run off buffer */
					data_bounds = TRUE;
					x -= d;		/* ignore this run */
					btr = d / 2 + (d & 1)
						+ (((d + 1) & 2) >> 1);
					for (; btr > 0; btr--) {
						if ((e = zgetc(zf)) == EOF)
							goto data_short;
					}
					continue;
				}
				for (i = d / 2; i > 0; i--) {
					if ((e = zgetc(zf)) == EOF)
						goto data_short;
					*(data++) = e >> 4;
					*(data++) = e & 0xf;
				}
				if (d & 1) {
					if ((e = zgetc(zf)) == EOF)
						goto data_short;
					*(data++) = e >> 4;
				}
				if ((d + 1) & 2)	/* read pad byte */
					if (zgetc(zf) == EOF)
						goto data_short;
			}
		} else {	/* No 4 bit rle compression */
			int d, s, p;
			int i, e;
			d = image->width / 2;	/* double pixel count */
			s = image->width & 1;	/* single pixel */
			p = (4 - (d + s)) & 0x3;	/* byte pad */
			for (y = image->height; y > 0; y--, data -= (2 * illen)) {
				for (i = d; i > 0; i--) {
					if ((e = zgetc(zf)) == EOF)
						goto data_short;
					*(data++) = e >> 4;
					*(data++) = e & 0xf;
				}
				if (s) {
					if ((e = zgetc(zf)) == EOF)
						goto data_short;
					*(data++) = e >> 4;
				}
				for (i = p; i > 0; i--)
					if (zgetc(zf) == EOF)
						goto data_short;
			}
		}
	} else if (hdr.biBitCount == 8) {
		byte *data;
		int illen, x, y;

		illen = image->width;
		/* start at bottom */
		data = image->data + (image->height - 1) * illen;

		if (hdr.biCompression == BI_RLE8) {
			int d, e;
			bzero((char *) image->data,
				image->width * image->height);
			for (x = y = 0;;) {
				if ((d = zgetc(zf)) == EOF)
					goto data_short;
				if (d != 0) {	/* run of pixels */
					x += d;
					if (x > image->width ||
							y > image->height) {
						/* don't run off buffer */
						data_bounds = TRUE;
						/* ignore this run */
						x -= d;
						if ((e = zgetc(zf)) == EOF)
							goto data_short;
						continue;
					}
					if ((e = zgetc(zf)) == EOF)
						goto data_short;
					bfill(data, d, e);
					data += d;
					continue;
				}
				/* else code */
				if ((d = zgetc(zf)) == EOF)
					goto data_short;
				if (d == 0) {	/* end of line */
					data -= (x + illen);
					x = 0;
					y++;
					continue;
				}
				/* else */
				if (d == 1)	/* end of bitmap */
					break;
				/* else */
				if (d == 2) {	/* delta */
					if ((d = zgetc(zf)) == EOF ||
							(e = zgetc(zf)) == EOF)
						goto data_short;
					x += d;
					data += d;
					y += e;
					data -= (e * illen);
					continue;
				}
				/* else run of literals */
				x += d;
				if (x > image->width || y > image->height) {
					int btr;
					/* don't run off buffer */
					data_bounds = TRUE;
					/* ignore this run */
					x -= d;	
					btr = d + (d & 1);
					for (; btr > 0; btr--) {
						if ((e = zgetc(zf)) == EOF)
							goto data_short;
					}
					continue;
				}
				if (zread(zf, data, d) != d)
					goto data_short;
				data += d;
				if (d & 1)	/* read pad byte */
					if (zgetc(zf) == EOF)
						goto data_short;
			}
		} else {	/* No 8 bit rle compression */
			byte pad[4];
			int padlen;

			/* extra bytes to word boundary */
			padlen = ((image->width + 3) & ~3) - illen;
			for (y = image->height; y > 0; y--, data -= illen) {
				if (zread(zf, data, illen) != illen
				    || zread(zf, pad, padlen) != padlen)
					goto data_short;
			}
		}
	} else {		/* hdr.biBitCount == 24 */
		byte *data, pad[4];
		int illen, padlen, y;
		illen = image->width * image->pixlen;
		/* extra bytes to word boundary */
		padlen = (((image->width * 3) + 3) & ~3) - illen;
		/* start at bottom */	
		data = image->data + (image->height - 1) * illen;	
		for (y = image->height; y > 0; y--, data -= illen) {
			int i;

			/* BMP files are RGB, so read straight in. */
			if (zread(zf, data, illen) != illen
			    || zread(zf, pad, padlen) != padlen)
				goto data_short;
			/* Oh, no they're not */
			for (i = 3 * image->width - 1; i > 0; i -= 3) {
				int t = data[i];
				data[i] = data[i - 2];
				data[i - 2] = t;
			}
		}

	}
	if (data_bounds)
		fprintf(stderr, "bmpLoad: %s - Data outside image area\n",
			hdr.name);
	zclose(zf);
	return image;

      data_short:
	fprintf(stderr, "bmpLoad: %s - Short read within Data\n", hdr.name);
	zclose(zf);
	return image;
}
