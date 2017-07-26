/* imagetypes.c:
 *
 * this contains the ImageTypes array
 *
 * jim frost 09.27.89
 *
 * Copyright 1989, 1991 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */

#include "copyright.h"
#include "xli.h"
#include "imagetypes.h"
#include <errno.h>

extern int errno;

/* some of these are order-dependent */

static struct {
	int (*identifier) (char *, char *);
	Image *(*loader) (char *, ImageOptions *, boolean);
	char *name;
} ImageTypes[] = {
	{fbmIdent,	fbmLoad, 	"FBM Image"},
	{sunRasterIdent, sunRasterLoad,	"Sun Rasterfile"},
	{cmuwmIdent,	cmuwmLoad,	"CMU WM Raster"},
	{pbmIdent,	pbmLoad,	"Portable Bit Map (PBM, PGM, PPM)"},
	{facesIdent,	facesLoad,	"Faces Project"},
	{pngIdent,	pngLoad,	"Portable Network Graphics (PNG)"},
	{gifIdent,	gifLoad,	"GIF Image"},
	{jpegIdent,	jpegLoad,	"JFIF style jpeg Image"},
	{rleIdent,	rleLoad,	"Utah RLE Image"},
	{bmpIdent,	bmpLoad,	"Windows, OS/2 RLE Image"},
	{pcdIdent,	pcdLoad,	"Photograph on CD Image"},
	{xwdIdent,	xwdLoad,	"X Window Dump"},
	{tgaIdent,	tgaLoad,	"Targa Image"},
	{mcidasIdent,	mcidasLoad,	"McIDAS areafile"},
	{g3Ident,	g3Load,		"G3 FAX Image"},
	{pcxIdent,	pcxLoad,	"PC Paintbrush Image"},
	{imgIdent,	imgLoad,	"GEM Bit Image"},
	{macIdent,	macLoad,	"MacPaint Image"},
	{xpixmapIdent,	xpixmapLoad,	"X Pixmap"},
	{xbitmapIdent,	xbitmapLoad,	"X Bitmap"},
	{NULL,		NULL,		NULL}
};


/* load a named image */
Image *loadImage(ImageOptions * image_ops, boolean verbose)
{
	char fullname[BUFSIZ];
	Image *image;
	int a;

	if (findImage(image_ops->name, fullname) < 0) {
		if (errno == ENOENT)
			printf("%s: image not found\n", image_ops->name);
		else if (errno == EISDIR)
			printf("%s: directory\n", image_ops->name);
		else
			perror(fullname);
		return (NULL);
	}

	image_ops->fullname = lmalloc(strlen(fullname) + 1);
	strcpy(image_ops->fullname, fullname);

	/* We've done this before !! */
	if (image_ops->loader_idx != -1) {
		image = ImageTypes[image_ops->loader_idx].loader(fullname,
			image_ops, verbose);
		if (image) {
			zreset(NULL);
			return (image);
		}
	} else {
		for (a = 0; ImageTypes[a].loader; a++) {
			image = ImageTypes[a].loader(fullname, image_ops,
				verbose);
			if (image) {
				zreset(NULL);
				return (image);
			}
		}
	}
	printf("%s: unknown or unsupported image type\n", fullname);
	zreset(NULL);
	return (NULL);
}

/* identify what kind of image a named image is */
void identifyImage(char *name)
{
	char fullname[BUFSIZ];
	int a;

	if (findImage(name, fullname) < 0) {
		if (errno == ENOENT)
			printf("%s: image not found\n", name);
		else if (errno == EISDIR)
			printf("%s: directory\n", name);
		else
			perror(fullname);
		return;
	}
	for (a = 0; ImageTypes[a].identifier; a++) {
		if (ImageTypes[a].identifier(fullname, name)) {
			zreset(NULL);
			return;
		}
	}
	zreset(NULL);
	printf("%s: unknown or unsupported image type\n", fullname);
}

/* tell user what image types we support */
void supportedImageTypes(void)
{
	int a;

	printf("Image types supported:\n");
	for (a = 0; ImageTypes[a].name; a++)
		printf("  %s\n", ImageTypes[a].name);
}
