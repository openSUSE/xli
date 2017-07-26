/* JPEG code for libjpeg v6
 * derived from xli 1.16 jpeg.c and IJG code
 * any bugs are my fault -- smar@reptiles.org
 */

#include "xli.h"
#include "imagetypes.h"
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>
#include <assert.h>
#include <ctype.h>

/* JFIF defines the gamma of pictures to be 1.0.  Unfortunately no-one
 * takes any notice (sigh), and the majority of images are like gifs -
 * they are adjusted to look ok on a "typical" monitor using a gamma naive
 * viewer/windowing system.  We give in to the inevitable here and use 2.2
 */
#define RETURN_GAMMA DEFAULT_IRGB_GAMMA

#define INPUT_BUF_SIZE 4096

typedef struct {
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
} xli_jpg_err;

typedef struct {
	struct jpeg_source_mgr pub;
	ZFILE *zfp;
	JOCTET buf[INPUT_BUF_SIZE];
} xli_jpg_src;


static void xli_jpg_error_exit(j_common_ptr cinfo)
{
	xli_jpg_err *err = (xli_jpg_err *) cinfo->err;

	(*cinfo->err->output_message) (cinfo);
	longjmp(err->setjmp_buffer, 1);
}


static void xli_jpg_init_src(j_decompress_ptr cinfo)
{
}


static boolean xli_jpg_fill_buf(j_decompress_ptr cinfo)
{
	xli_jpg_src *src = (xli_jpg_src *) cinfo->src;
	size_t n;

	n = zread(src->zfp, src->buf, INPUT_BUF_SIZE);

	if (n <= 0) {
		WARNMS(cinfo, JWRN_JPEG_EOF);
		/* Insert a fake EOI marker */
		src->buf[0] = (JOCTET) 0xFF;
		src->buf[1] = (JOCTET) JPEG_EOI;
		n = 2;
	}
	src->pub.next_input_byte = src->buf;
	src->pub.bytes_in_buffer = n;

	return TRUE;
}


static void xli_jpg_skip(j_decompress_ptr cinfo, long int n)
{
	xli_jpg_src *src = (xli_jpg_src *) cinfo->src;

	if (n > 0) {
		while (n > (long) src->pub.bytes_in_buffer) {
			n -= (long) src->pub.bytes_in_buffer;
			xli_jpg_fill_buf(cinfo);
		}
		src->pub.next_input_byte += (size_t) n;
		src->pub.bytes_in_buffer -= (size_t) n;
	}
}


/*
maybe in future we can exploit zio's rewind capability to
provide a resync_to_restart function here?
*/


static void xli_jpg_term(j_decompress_ptr cinfo)
{
	/* return unread input to the zstream */
	xli_jpg_src *src = (xli_jpg_src *) cinfo->src;

	if (src->zfp && src->pub.bytes_in_buffer > 0) {
		zunread(src->zfp, (byte *) src->pub.next_input_byte,
			src->pub.bytes_in_buffer);
	}
}


static void xli_jpg_zio_src(j_decompress_ptr cinfo, ZFILE *zfp)
{
	xli_jpg_src *src;

	if (!cinfo->src) {
		cinfo->src = (struct jpeg_source_mgr *)
		    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
						sizeof(xli_jpg_src));
	}
	src = (xli_jpg_src *) cinfo->src;
	src->pub.init_source = xli_jpg_init_src;
	src->pub.fill_input_buffer = xli_jpg_fill_buf;
	src->pub.skip_input_data = xli_jpg_skip;
	src->pub.resync_to_restart = jpeg_resync_to_restart;	/* use default */
	src->pub.term_source = xli_jpg_term;
	src->zfp = zfp;
	src->pub.bytes_in_buffer = 0;	/* forces fill_input_buffer on first read */
	src->pub.next_input_byte = NULL;	/* until buffer loaded */
}


static void describe_jpeg(j_decompress_ptr cinfo, char *filename)
{
	printf("%s is a %dx%d JPEG image, color space ", filename,
	       cinfo->image_width, cinfo->image_height);

	switch (cinfo->jpeg_color_space) {
	case JCS_UNKNOWN:
		printf("Unknown");
		break;
	case JCS_GRAYSCALE:
		printf("Grayscale");
		break;
	case JCS_RGB:
		printf("RGB");
		break;
	case JCS_YCbCr:
		printf("YCbCr");
		break;
	case JCS_CMYK:
		printf("CMYK");
		break;
	case JCS_YCCK:
		printf("YCCK");
		break;
	default:
		printf("Totally Weird");
		break;
	}

	printf(", %d comp%s, %s\n", cinfo->num_components,
	       cinfo->num_components ? "s." : ".",
	     cinfo->arith_code ? "Arithmetic coding" : "Huffman coding");
}


static unsigned int xli_jpg_getc(j_decompress_ptr cinfo)
{
	struct jpeg_source_mgr *datasrc = cinfo->src;

	if (datasrc->bytes_in_buffer == 0)
		(*datasrc->fill_input_buffer) (cinfo);
	datasrc->bytes_in_buffer--;

	return GETJOCTET(*datasrc->next_input_byte++);
}


static boolean xli_jpg_com(j_decompress_ptr cinfo)
{
	long length;
	unsigned int ch;
	unsigned int lastch = 0;

	length = xli_jpg_getc(cinfo) << 8;
	length += xli_jpg_getc(cinfo);
	length -= 2;		/* discount the length word itself */

	printf("Comment, length %ld:\n", (long) length);

	while (--length >= 0) {
		ch = xli_jpg_getc(cinfo);
		/* Emit the character in a readable form.
		   * Nonprintables are converted to \nnn form,
		   * while \ is converted to \\.
		   * Newlines in CR, CR/LF, or LF form will be printed as one newline.
		 */
		if (ch == '\r') {
			printf("\n");
		} else if (ch == '\n') {
			if (lastch != '\r')
				printf("\n");
		} else {
			putc(ch, stdout);
		}
		lastch = ch;
	}

	printf("\n");

	return TRUE;
}


Image *jpegLoad(char *fullname, ImageOptions *image_ops, boolean verbose)
{
	ZFILE *zfp;
	struct jpeg_decompress_struct cinfo;
	xli_jpg_err jerr;
	Image *image = 0;
	byte **rows = 0;
	int i, rowbytes;

	CURRFUNC("jpegLoad");
	zfp = zopen(fullname);
	if (!zfp)
		return (Image *) 0;

	/* Quick check to see if file starts with JPEG SOI marker */
	if (zgetc(zfp) != 0xFF || zgetc(zfp) != 0xD8) {
		zclose(zfp);
		return (Image *) 0;
	}
	zrewind(zfp);

	/* set default error handlers and override error_exit */
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = xli_jpg_error_exit;

	if (setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&cinfo);
		zclose(zfp);
		if (rows) {
			lfree((byte *) rows);
			rows = 0;
		}
		return image;
	}
	jpeg_create_decompress(&cinfo);
	xli_jpg_zio_src(&cinfo, zfp);

	if (verbose)
		jpeg_set_marker_processor(&cinfo, JPEG_COM, xli_jpg_com);

	jpeg_read_header(&cinfo, TRUE);
	if (verbose)
		describe_jpeg(&cinfo, fullname);

	if (image_ops->iscale > 0 && image_ops->iscale < 4) {
		cinfo.scale_num = 1;
		cinfo.scale_denom = 1 << image_ops->iscale;
	} else if (image_ops->iscale_auto) {
		image_ops->iscale = 0;
		while (image_ops->iscale < 3 && (cinfo.image_width >>
				image_ops->iscale > globals.dinfo.width * .9 ||
				cinfo.image_height >> image_ops->iscale >
				globals.dinfo.height * .9))
			image_ops->iscale += 1;
		cinfo.scale_denom = 1 << image_ops->iscale;
		if (verbose)
			printf("auto-scaling to 1/%d\n", cinfo.scale_denom);
	}
	znocache(zfp);

	jpeg_start_decompress(&cinfo);

	if (JCS_GRAYSCALE == cinfo.out_color_space) {
		int i;

		image = newRGBImage(cinfo.output_width, cinfo.output_height, 8);
		image->title = dupString(image_ops->name);
		for (i = 0; i < 256; i++) {
			image->rgb.red[i] = image->rgb.green[i] =
			    image->rgb.blue[i] = i << 8;
		}
		image->rgb.used = 256;
	} else if (JCS_RGB == cinfo.out_color_space) {
		image = newTrueImage(cinfo.output_width, cinfo.output_height);
		image->title = dupString(image_ops->name);
	} else {
		fprintf(stderr, "jpegLoad: weird output color space\n");
		jpeg_destroy_decompress(&cinfo);
		zclose(zfp);

		return (Image *) 0;
	}

	image->gamma = RETURN_GAMMA;
	if (cinfo.scale_denom > 1)
		image->flags |= FLAG_ISCALE;

	rowbytes = cinfo.output_width * cinfo.output_components;
	assert(image->pixlen * image->width == rowbytes);

	rows = (byte **) lmalloc(image->height * sizeof(byte *));
	for (i = 0; i < image->height; ++i)
		rows[i] = image->data + i * rowbytes;

	while (cinfo.output_scanline < cinfo.output_height) {
		jpeg_read_scanlines(&cinfo, rows + cinfo.output_scanline,
			    cinfo.output_height - cinfo.output_scanline);
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	read_trail_opt(image_ops, zfp, image, verbose);
	zclose(zfp);
	lfree((byte *) rows);
	rows = 0;

	return image;
}


int jpegIdent(char *fullname, char *name)
{
	ZFILE *zfp;
	struct jpeg_decompress_struct cinfo;
	xli_jpg_err jerr;

	CURRFUNC("jpegIdent");
	zfp = zopen(fullname);
	if (!zfp)
		return 0;

	/* Quick check to see if file starts with JPEG SOI marker */
	if (zgetc(zfp) != 0xFF || zgetc(zfp) != 0xD8) {
		zclose(zfp);
		return 0;
	}
	zrewind(zfp);

	/* set default error handlers and override error_exit */
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = xli_jpg_error_exit;

	if (setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&cinfo);
		zclose(zfp);
		return 0;
	}
	jpeg_create_decompress(&cinfo);
	xli_jpg_zio_src(&cinfo, zfp);

	jpeg_read_header(&cinfo, TRUE);
	describe_jpeg(&cinfo, fullname);
	jpeg_destroy_decompress(&cinfo);
	zclose(zfp);

	return 1;
}
