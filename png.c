/*
 * Based on example.c from libpng by Guy Eric Schalnat, Andreas Dilger,
 * Glenn Randers-Pehrson et al.  Any bugs are my fault. -- smar@reptiles.org
 */

#include "xli.h"
#include "imagetypes.h"
#include "pbm.h"
#include <png.h>
#include <assert.h>

#define TITLE_KEYWORD "Title"

/* check to see if a file is a png file using png_sig_cmp() */
static int check_png(char *file_name)
{
	ZFILE *zfp;
	char buf[8];
	int ret;

	zfp = zopen(file_name);
	if (!zfp)
		return 0;
	ret = zread(zfp, buf, 8);
	zclose(zfp);

	if (ret != 8)
		return 0;

	ret = !png_sig_cmp(buf, 0, 8);

	return (ret);
}


static void describe_png(char *name, png_struct *png, png_info *info)
{
	int bd, ct;

	ct = png_get_color_type(png, info);
	bd = png_get_bit_depth(png, info);

	printf("%s is a%s %lux%lu %d bit deep %s PNG image",
		name,
		png_get_interlace_type(png, info) ? "n interlaced" : "",
		png_get_image_width(png, info),
		png_get_image_height(png, info),
		bd,
		(ct & PNG_COLOR_MASK_PALETTE) ? "paletted" :
			(ct & PNG_COLOR_MASK_COLOR) ? "RGB" : "grayscale");
	if (ct & PNG_COLOR_MASK_ALPHA)
		printf(" with an alpha channel");
	if (png_get_valid(png, info, PNG_INFO_tRNS))
		printf(" with transparency");
	if (png_get_valid(png, info, PNG_INFO_bKGD)) {
		png_color_16 *bg;
		int d;

		png_get_bKGD(png, info, &bg);
		d = (bd + 3) / 4;
		printf(", background = ");
		if (!(ct & PNG_COLOR_MASK_COLOR)) {
			printf("#%0*x", d, bg->gray);
		} else if (PNG_COLOR_TYPE_PALETTE == ct) {
			png_color *bp;
			int np;

			png_get_PLTE(png, info, &bp, &np);
			bp += bg->index;
			printf("index %d: #%02x%02x%02x", bg->index,
			       bp->red, bp->green, bp->blue);
		} else {
			printf("#%0*x%0*x%0*x", d, bg->red, d, bg->green,
				d, bg->blue);
		}
	}
	if (png_get_valid(png, info, PNG_INFO_gAMA)) {
		double g;

		png_get_gAMA(png, info, &g);
		printf(", gamma = %g", g);
	}
	printf("\n");
}


static void xli_png_read_data(png_struct *png, png_byte *data,
	png_size_t length)
{
	ZFILE *zfp;

	zfp = (ZFILE *) png_get_io_ptr(png);
	if (zread(zfp, (byte *) data, (int) length) != length)
		png_error(png, "unexpected EOF in xli_png_read_data");
}


static void xli_png_error(png_struct * png_ptr, png_const_charp error_msg)
{
	fputs(error_msg, stderr);
	putc('\n', stderr);
	longjmp(*((jmp_buf *) png_get_error_ptr(png_ptr)), 1);
}


Image *pngLoad(char *fullname, ImageOptions * opt, boolean verbose)
{
	ZFILE *zfp;
	png_struct *png;
	png_info *info;
	Image *image = (Image *) 0;
	int orig_depth = 0;
	byte **row = (byte **) 0;
	jmp_buf jmpbuf;
	int bit_depth, color_type;

	/* open the file */
	if (!check_png(fullname))
		return (Image *) 0;

	zfp = zopen(fullname);
	if (!zfp) {
		perror("pngLoad");
		return (Image *) 0;
	}
	png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
		(void *) &jmpbuf, xli_png_error, (png_error_ptr) 0);

	if (!png) {
		zclose(zfp);
		return (Image *) 0;
	}

	info = png_create_info_struct(png);
	if (!info) {
		zclose(zfp);
		png_destroy_read_struct(&png, (png_infopp) 0, (png_infopp) 0);
		return (Image *) 0;
	}

	/* scary non-local transfer of control action */
	if (setjmp(jmpbuf)) {
		png_destroy_read_struct(&png, &info, (png_infopp) 0);
		zclose(zfp);
		if (row)
			lfree((byte *) row);
		return image;
	}

	/* set up the input control */
	png_set_read_fn(png, (void *) zfp, xli_png_read_data);

	/* read the file information */
	png_read_info(png, info);

	if (verbose)
		describe_png(fullname, png, info);

	znocache(zfp);

	/* the rules:
	   bit depth over 8        ITRUE
	   rgb colour              ITRUE
	   1 bit gray              IBITMAP
	   other gray              IRGB
	   palette colour          IRGB
	 */

	bit_depth = png_get_bit_depth(png, info);
	color_type = png_get_color_type(png, info);
	if (bit_depth < 8 && (PNG_COLOR_TYPE_RGB == color_type ||
			PNG_COLOR_TYPE_RGB_ALPHA == color_type))
		png_set_expand(png);

	/* Set the background color to draw transparent and alpha images over */
	if ((color_type & PNG_COLOR_MASK_ALPHA) ||
			(png_get_valid(png, info, PNG_INFO_tRNS) && (opt->bg ||
			png_get_valid(png, info, PNG_INFO_bKGD)))) {
		png_color_16 bg;
		int expand = 0;
		int gflag = PNG_BACKGROUND_GAMMA_SCREEN;
		double gval = 1.0;

		bg.red = bg.green = bg.blue = bg.gray = 0;
		if (PNG_COLOR_TYPE_PALETTE == color_type || opt->bg)
			png_set_expand(png);

		if (opt->bg) {
			XColor xc;
			int shift = ((color_type & PNG_COLOR_MASK_ALPHA)
				&& 16 == bit_depth) ? 0 : 8;

			xc.flags = DoRed | DoGreen | DoBlue;
			xliParseXColor(&(globals.dinfo), opt->bg, &xc);
			bg.red = xc.red >> shift;
			bg.green = xc.green >> shift;
			bg.blue = xc.blue >> shift;
			bg.gray = (int) (xc.red * .299 + xc.green * .587 +
				xc.blue * .114 + .5) >> shift;
		} else if (png_get_valid(png, info, PNG_INFO_bKGD)) {
			png_color_16 *bgp;

			png_get_bKGD(png, info, &bgp);
			bg = *bgp;
			expand = 1;
			gflag = PNG_BACKGROUND_GAMMA_FILE;
		}
		png_set_background(png, &bg, gflag, expand, gval);
		png_set_strip_alpha(png);
	}
	if (bit_depth < 8 && (bit_depth > 1 ||
			PNG_COLOR_TYPE_GRAY != color_type)) {
		if (PNG_COLOR_TYPE_GRAY == color_type)
			orig_depth = bit_depth;
		png_set_packing(png);
	}
	/* tell libpng to strip 16 bit depth files down to 8 bits */
	if (bit_depth > 8)
		png_set_strip_16(png);

	if (png_get_interlace_type(png, info))
		png_set_interlace_handling(png);

	/* update palette with transformations, update the info structure */
	png_read_update_info(png, info);
	bit_depth = png_get_bit_depth(png, info);
	color_type = png_get_color_type(png, info);

	/* allocate the memory to hold the image using the fields
	 *  of png_info.
	 */
	if (PNG_COLOR_TYPE_GRAY == color_type && 1 == bit_depth) {
		image = newBitImage(png_get_image_width(png, info),
			png_get_image_height(png, info));
		png_set_invert_mono(png);
	} else if (PNG_COLOR_TYPE_PALETTE == color_type) {
		int i, np;
		png_color *pp;

		image = newRGBImage(png_get_image_width(png, info),
			png_get_image_height(png, info), bit_depth);
		png_get_PLTE(png, info, &pp, &np);
		for (i = 0; i < np; ++i) {
			image->rgb.red[i] = pp[i].red * 0x101;
			image->rgb.green[i] = pp[i].green * 0x101;
			image->rgb.blue[i] = pp[i].blue * 0x101;
		}
		image->rgb.used = np;
	} else if (PNG_COLOR_TYPE_GRAY == color_type) {
		int i;
		int depth = orig_depth ? orig_depth : bit_depth;
		int maxval = (1 << depth) - 1;

		image = newRGBImage(png_get_image_width(png, info),
			png_get_image_height(png, info), depth);
		for (i = 0; i <= maxval; i++) {
			image->rgb.red[i] = PM_SCALE(i, maxval, 0xffff);
			image->rgb.green[i] = PM_SCALE(i, maxval, 0xffff);
			image->rgb.blue[i] = PM_SCALE(i, maxval, 0xffff);
		}
		image->rgb.used = maxval + 1;
	} else {
		image = newTrueImage(png_get_image_width(png, info),
			png_get_image_height(png, info));
	}

	if (png_get_valid(png, info, PNG_INFO_gAMA) && image->type != IBITMAP) {
		double g;

		png_get_gAMA(png, info, &g);
		image->gamma = 1.0 / g;
	}

	/* read the image */
	if (IBITMAP == image->type) {
		assert((image->width + 7) / 8 == png_get_rowbytes(png, info));
	} else {
		assert(image->width * image->pixlen ==
			png_get_rowbytes(png, info));
	}

	{
		int i;

		row = (byte **) lmalloc(image->height * sizeof(byte *));
		for (i = 0; i < image->height; ++i)
			row[i] = image->data + i * png_get_rowbytes(png, info);
	}

	png_read_image(png, row);

	/* read the rest of the file, getting any additional chunks in info */
	png_read_end(png, info);

	if (verbose && png_get_valid(png, info, PNG_INFO_tIME)) {
		png_time *tp;

		png_get_tIME(png, info, &tp);
		printf("Modification time: %d/%d/%d %02d:%02d:%02d",
		       tp->year, tp->month, tp->day,
		       tp->hour, tp->minute, tp->second);
	}

	{
		int i, num_text;
		char *title;
		png_text *text;

		title = opt->name;
		num_text = png_get_text(png, info, &text, (int *) 0);
		for (i = 0; i < num_text; ++i) {
			if (verbose)
				printf("\n%s: %s\n", text[i].key, text[i].text);
			if (!strcmp(text[i].key, TITLE_KEYWORD))
				title = text[i].text;
		}
		image->title = dupString(title);
	}

	/* clean up after the read, and free any memory allocated */
	png_destroy_read_struct(&png, &info, (png_infopp) 0);

	/* free the structures */
	lfree((byte *) row);

	/* close the file */
	zclose(zfp);

	/* that's it */
	return image;
}


int pngIdent(char *fullname, char *name)
{
	ZFILE *zfp;
	png_struct *png;
	png_info *info;
	jmp_buf jmpbuf;

	if (!check_png(fullname))
		return 0;

	/* open the file */
	zfp = zopen(fullname);
	if (!zfp) {
		perror("pngIdent");
		return 0;
	}
	/* allocate the necessary structures */
	png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
		(void *) &jmpbuf, xli_png_error, (png_error_ptr) 0);

	if (!png) {
		zclose(zfp);
		return 0;
	}
	info = png_create_info_struct(png);
	if (!info) {
		zclose(zfp);
		png_destroy_read_struct(&png, (png_infopp) 0, (png_infopp) 0);
		return 0;
	}
	/* set error handling */
	if (setjmp(jmpbuf)) {
		png_destroy_read_struct(&png, &info, (png_infopp) 0);
		zclose(zfp);
		return 0;
	}
	/* set up the input control */
	png_set_read_fn(png, (void *) zfp, xli_png_read_data);

	/* read the file information */
	png_read_info(png, info);

	describe_png(fullname, png, info);

	/* clean up after the read, and free any memory allocated */
	png_destroy_read_struct(&png, &info, (png_infopp) 0);

	/* close the file */
	zclose(zfp);

	/* that's it */
	return 1;
}
