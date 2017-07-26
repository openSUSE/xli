/*
 * Read a Photograph on CD format image.
 *
 * Created for xli by Graeme Gill,
 *
 * based on information (and some code) from xvphotocd by David Clunie,
 *
 * which was based on information from hpcdtoppm by Hadmut Danisch.
 *
 */

#include "xli.h"
#include "imagetypes.h"
#include "pcd.h"

#undef TEST_DELTA

static int read_baseband(pcdHeader *hp, int toff, int sf);
static void upsample(pcdHeader *hp, byte *data, int sf);
static void pcd_ycc_rgb_init(void);
static void pcd_ycc_to_rgb(pcdHeader *hp, Image *image);
static void rot_block(byte *Lp, byte *C1p, byte *C2p, int hii, int vii,
	byte *op, int voi, int w, int h);
static huff *gethufftable(pcdHeader *hp, int *size);
static int gethuffdata(pcdHeader * hp, int p, int tb, int db, int sf);

/* Read a block into the buffer and update the off counter. */
/* If size is > PCDBLOCK, skip blocks and read size % PCDBLOCK */
/* return TRUE on error */
static boolean pcd_read(pcdHeader *hp, long unsigned int size)
{
	/* Read in the header block */
	while (size > PCDBLOCK) {
		if (zread(hp->zf, hp->buf, PCDBLOCK) != PCDBLOCK)
			return TRUE;

		size -= PCDBLOCK;
		hp->off += hp->nob;
		hp->nob = PCDBLOCK;
	}
	if (zread(hp->zf, hp->buf, size) != size)
		return TRUE;
	hp->off += hp->nob;
	hp->nob = size;

	/* Reset the bit read stuff */
	hp->bp = hp->buf;
	hp->bytes_left = size;
	return FALSE;
}

/* Read a block into the specified buffer and update the off counter. */
/* return TRUE on error */
static boolean pcd_read_to_buf(pcdHeader *hp, byte *buf,
	long unsigned int size)
{
	if (zread(hp->zf, buf, size) != size)
		return TRUE;

	hp->off += (hp->nob + size);
	hp->nob = 0;

	/* Reset the bit read stuff */
	hp->bytes_left = 0;
	return FALSE;
}

/* Read data to align file with the given offset */
/* The next read will be data starting at that offset */
/* (Need to do this or pcd_skip_blocks() before using */
/* bit fetch macros) */
static boolean pcd_skip(pcdHeader * hp, long unsigned int toff)
{
	if (toff < pcd_offset(hp)) {
		fprintf(stderr,
			"pcdLoad: internal error - tried to seek backwards\n");
		return TRUE;
	}

	if (toff > pcd_offset(hp))
		if (pcd_read(hp, toff - pcd_offset(hp)))
			return TRUE;

	/* Clear the byte/bit read stuff, */
	/* so it will fetch from aligned address. */
	hp->bits = 0;
	hp->bytes_left = 0;
	hp->bits_left = 0;

	return FALSE;
}

/* Skip to the end of the current block. */
static boolean pcd_skip_eob(pcdHeader * hp)
{
	unsigned int toff;
	toff = ((pcd_offset(hp) + PCDBMASK) >> PCDSHIFT);
	return (pcd_skip(hp, toff * PCDBLOCK));
}

/* Read the header of the file, and */
/* Return TRUE if this looks like a pcd file */
static boolean read_pcdHeader(pcdHeader * hp, char *name)
{
	int i, w, h, sz;

	CURRFUNC("read_pcdHeader");
	if (zread(hp->zf, hp->buf, 32) != 32)
		return FALSE;

	/* See if it is consistent with a pcd files */
	for (i = 0; i < 32; i++)
		if (hp->buf[i] != 0xff)
			return FALSE;

	znocache(hp->zf);

	/* Read in the rest of the block */
	if (zread(hp->zf, &hp->buf[32], PCDBLOCK - 32) != PCDBLOCK - 32)
		return FALSE;

	hp->off = 0;
	hp->nob = PCDBLOCK;

	/* Read in the header block */
	if (pcd_read(hp, PCDBLOCK))
		return FALSE;

	/* Image format ident string (?) */
	bcopy(&hp->buf[0x0800 & PCDBMASK], hp->magic, 7);
	hp->magic[7] = '\000';
	if (strcmp(hp->magic, "PCD_IPI") != 0)
		return FALSE;	/* ? */

	/* Source (?) */
	bcopy(&hp->buf[0x0816 & PCDBMASK], hp->source, 88);
	hp->source[88] = '\000';

	/* Owner (?) */
	bcopy(&hp->buf[0x0870 & PCDBMASK], hp->owner, 20);
	hp->owner[20] = '\000';

	/* Image orientation */
	switch (hp->buf[0x0e02 & PCDBMASK] & 3) {
	case 0:
		hp->rotate = PCD_NO_ROTATE;
		break;
	case 1:
		hp->rotate = PCD_ACLOCK_ROTATE;
		break;
	case 3:
		hp->rotate = PCD_CLOCK_ROTATE;
		break;
	default:
		fprintf(stderr, "pcd: warning, unrecognized image orientation\n");
		hp->rotate = PCD_NO_ROTATE;
		break;
	}

	/* Figure out the default size of the image (allowing for rotation) */
	if (hp->rotate == PCD_NO_ROTATE)
		w = BASEW * 4, h = BASEH * 4;
	else
		w = BASEH * 4, h = BASEW * 4;
	for (sz = SZ_16B;; sz--, w /= 2, h /= 2) {
		if (sz == SZ_B16
		    || (w <= hp->width && h <= hp->height
		     && ((w * 2) > hp->width || (h * 2) > hp->height))) {
			hp->size = sz;
			hp->width = w;
			hp->height = h;
			break;
		}
	}

	/* Re-compute for best quality zoom              */
	/* (should actually compute zoom up for speed,   */
	/*  and zoom down when "quality" flag selected.) */
	if ((hp->xzoom != 0 && hp->xzoom != 100)
	    || (hp->yzoom != 0 && hp->yzoom != 100)) {
		int tw, th;

		if (hp->xzoom != 0 && hp->xzoom != 100)
			tw = (int) ((float) hp->width *
				(float) hp->xzoom / 100.0 + 0.5);
		else
			tw = hp->width;

		if (hp->yzoom != 0 && hp->yzoom != 100)
			th = (int) ((float) hp->height *
				(float) hp->yzoom / 100.0 + 0.5);
		else
			th = hp->height;

		/* Set for PCD size larger than requested size */
		if (hp->rotate == PCD_NO_ROTATE)
			w = BASEW / 4, h = BASEH / 4;
		else
			w = BASEH / 4, h = BASEW / 4;
		for (sz = SZ_B16;; sz++, w *= 2, h *= 2) {
			if (sz == SZ_16B || (w >= tw && h >= th)) {
				hp->size = sz;
				hp->width = w;
				hp->height = h;
				hp->xzoom = (unsigned int) ((float) tw /
					(float) w * 100.0 + 0.5);
				hp->yzoom = (unsigned int) ((float) th /
					(float) h * 100.0 + 0.5);
				break;
			}
		}
	}

	/* Return un-rotated dimentions */
	if (hp->rotate != PCD_NO_ROTATE) {
		int t;
		t = hp->width;
		hp->width = hp->height;
		hp->height = t;
	}
	hp->name = name;
	return TRUE;
}

/* Print a brief description of the image */
static void tell_about_pcd(pcdHeader * hp)
{
	printf("%s is a %dx%d Photograph on CD Image%s\n",
	       hp->name,
	       hp->rotate == PCD_NO_ROTATE ? hp->width : hp->height,
	       hp->rotate == PCD_NO_ROTATE ? hp->height : hp->width,
	       hp->rotate == PCD_ACLOCK_ROTATE ? " (On right side)" :
	       hp->rotate == PCD_CLOCK_ROTATE ? " (On left side)" : "");
}

int pcdIdent(char *fullname, char *name)
{
	pcdHeader hdr;

	CURRFUNC("pcdIdent");
	if (!(hdr.zf = zopen(fullname))) {
		perror("pcdIdent");
		return (0);
	}
	/* Set a target width and height to the display size */
	hdr.width = globals.dinfo.width;
	hdr.height = globals.dinfo.height;

	if (!read_pcdHeader(&hdr, name)) {
		zclose(hdr.zf);
		return 0;	/* Nope, not a PCD file */
	}
	tell_about_pcd(&hdr);
	zclose(hdr.zf);
	return 1;
}

Image *pcdLoad(char *fullname, ImageOptions * image_ops, boolean verbose)
{
	pcdHeader hdr;
	Image *image;
	unsigned long toff;
	int sf;			/* target/current image size */

	CURRFUNC("pcdLoad");

	if (!(hdr.zf = zopen(fullname))) {
		perror("pcdIdent");
		return (0);
	}
	/* Set a target width and height to the */
	/* screen size. */
	hdr.width = globals.dinfo.width * 1.0;
	hdr.height = globals.dinfo.height * 1.0;
	hdr.xzoom = image_ops->xzoom;
	hdr.yzoom = image_ops->yzoom;

	if (!read_pcdHeader(&hdr, image_ops->name)) {
		zclose(hdr.zf);
		return NULL;	/* Nope, not a Photograph on CD file */
	}
	if (verbose)
		tell_about_pcd(&hdr);
	znocache(hdr.zf);

	image = newTrueImage(hdr.width, hdr.height);
	image->title = dupString(hdr.name);
	image_ops->xzoom = hdr.xzoom;
	image_ops->yzoom = hdr.yzoom;
	hdr.Lp = image->data;
	hdr.C1p = hdr.Lp + (image->width * image->height);
	hdr.C2p = hdr.C1p + (image->width * image->height);

	toff = 0;
	switch (hdr.size) {
	case SZ_B16:
		toff = 4 * PCDBLOCK;
		sf = 1;
		break;
	case SZ_B4:
		toff = 23 * PCDBLOCK;
		sf = 1;
		break;
	case SZ_B:
		toff = 96 * PCDBLOCK;
		sf = 1;
		break;
	case SZ_4B:
		toff = 96 * PCDBLOCK;
		sf = 2;
		break;
	case SZ_16B:
		toff = 96 * PCDBLOCK;
		sf = 4;
		break;
	default:
		fprintf(stderr, "pcdLoad: unknown size %d\n", hdr.size);
		goto data_error;
		break;
	}

	/* Read the baseband image */
	if (read_baseband(&hdr, toff, sf))
		goto data_error;

	/* Need to up-sample C1 and C2 */
	upsample(&hdr, hdr.C1p, sf);
	upsample(&hdr, hdr.C2p, sf);

	/* Up sample all components for greater than baseline sizes */
	if (sf >= 2) {
		if (pcd_skip(&hdr, 384 * PCDBLOCK))	/* skip to start of greater than base stuff */
			goto data_short;
		sf /= 2;
		upsample(&hdr, hdr.Lp, sf);	/* upsample luma */
		gethuffdata(&hdr, 1, 4, 5, sf);		/* Add luma delta */
		upsample(&hdr, hdr.C1p, sf);	/* upsample chroma */
		upsample(&hdr, hdr.C2p, sf);

		if (sf >= 2) {
			if (pcd_skip_eob(&hdr))		/* Round up to next block */
				goto data_short;
			sf /= 2;
			upsample(&hdr, hdr.Lp, sf);	/* upsample luma */
			gethuffdata(&hdr, 3, 12, 14, sf);	/* Add luma and chroma delta */
			upsample(&hdr, hdr.C1p, sf);	/* upsample chroma */
			upsample(&hdr, hdr.C2p, sf);
		}
	}
	/* Need to convert LC1C2 to RGB */
	pcd_ycc_to_rgb(&hdr, image);

	zclose(hdr.zf);
	image->gamma = 2.05;	/* Approximate CCIR 709 */
	return image;

      data_short:
	fprintf(stderr, "pcdLoad: %s - Short read within Data\n", hdr.name);
      data_error:
	zclose(hdr.zf);
	return image;
}


/* Read baseband image components in */
/* toff: File block offset to read components from */
/* sf: Scale factor down from image size */
int read_baseband(pcdHeader * hp, int toff, int sf)
{
	int h, w, hw;
	byte *Lp, *C1p, *C2p;
	w = hp->width / sf;	/* width */
	hw = w / 2;		/* Half width */

	/* skip data to start of appropriate block */
	if (pcd_skip(hp, toff))
		goto data_short;

	/* Read in an image */
	for (h = hp->height / (2 * sf), Lp = hp->Lp, C1p = hp->C1p, C2p = hp->C2p;
	     h > 0;
	     h--) {
		/* First row of luma */
		if (pcd_read_to_buf(hp, Lp, w))
			goto data_short;
		Lp += w;

		/* Second row of luma */
		if (pcd_read_to_buf(hp, Lp, w))
			goto data_short;
		Lp += w;

		/* C1 + C2 */
		if (pcd_read_to_buf(hp, C1p, hw))
			goto data_short;
		C1p += hw;
		if (pcd_read_to_buf(hp, C2p, hw))
			goto data_short;
		C2p += hw;
	}
	return FALSE;

      data_short:
	fprintf(stderr, "pcd: Short data read in baseband data\n");
	return TRUE;
}


/* xvphotocd upsample */
/* The input pixels are at the top left of each resulting four
 * pixels. Pixels directly between the input pixels are
 * the average of adjacent pixels. The remaing pixels
 * are the average of the four diagonal input pixels.
 */
void upsample(pcdHeader * hp, byte * data, int sf)
			/* start point */
			/* Output target/curret size */
{
	int x, y;
	int w, hw, h, hh;	/* Width, half width, height, half height */
	int v1, v2;		/* Upper input values */
	int v3, v4;		/* Lower input values */
	byte *ui, *li;		/* upper/lower input pointers */
	byte *uo, *lo;		/* upper/lower output pointers */

	w = hp->width / sf;
	hw = w / 2;
	h = hp->height / sf;
	hh = h / 2;

	ui = li = uo = lo = 0;
	for (y = 0; y < h;) {
		if (y == 0) {
			ui = li = data + (hw * hh) - 1;		/* last input pixel */
			uo = lo = data + (w * h) - 1;	/* last output pixel */
			y++;
		} else if (y == (h - 1)) {
			uo = lo;
			ui = li;
			y++;
		} else
			y += 2;

		/* First pixel */
		v2 = *li;
		li -= 1;
		v4 = *ui;
		ui -= 1;

		/* v2 = v2 */
		v4 = v4 + v2;

		/* First pixel in line */
		*lo = v2;
		lo -= 1;
		*uo = (v4 + 1) >> 1;
		uo -= 1;

		/* Do middle of double line two pixels at a time */
		for (x = 3; x < w; x += 2) {
			v1 = v2;
			v2 = *li;
			li -= 1;
			v3 = v4;
			v4 = *ui;
			ui -= 1;

			/* v2 = v2 */
			v4 = v4 + v2;

			*lo = v1;
			*(lo - 1) = (v1 + v2 + 1) >> 1;
			lo -= 2;

			*uo = (v3 + 1) >> 1;
			*(uo - 1) = (v3 + v4 + 2) >> 2;
			uo -= 2;
		}

		/* Last pixel in line */
		*lo = v2;
		*uo = (v4 + 1) >> 1;

		/* Bump input and output pointer to the next double line */
		/* ui = ui */
		li = ui + hw;
		lo = uo - 1;
		uo -= (w + 1);
	}
}

/*
 * LC1C2 to RGB
 * 
 * Currently just an approximation, since PCD
 * is (?) really CCIR 601-1 with CCIR 709 primaries,
 * and what is used below is just an approximation.
 * 
 * The following code is based on IJPG code.
 */

#define SCALEBITS	16
#define ONE_HALF	((int) 1 << (SCALEBITS-1))
#define FIX(x)		((int) ((x) * (1<<SCALEBITS) + 0.5))

/* This table rounds values to the range 0 to 255 */
/* Compression is used for input values between 230 and 346 */
byte roundtab[768] =
{
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
	80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	96, 97, 98, 99, 100,101,102,103,104,105,106,107,108,109,110,111,
	112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
	128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
	144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
	160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
	176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
	192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
	208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
	224,225,226,227,228,229,230,231,232,233,234,235,236,237,237,238,
	239,240,240,241,241,242,243,243,244,244,244,245,245,246,246,246,
	247,247,247,248,248,248,248,249,249,249,249,249,250,250,250,250,
	250,251,251,251,251,251,251,251,251,252,252,252,252,252,252,252,
	252,252,252,253,253,253,253,253,253,253,253,253,253,253,253,253,
	253,253,253,254,254,254,254,254,254,254,254,254,254,254,254,254,
	254,254,254,254,254,254,254,254,254,254,254,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
};

static byte *pcdlimit = NULL;	/* limit table */
static byte *pcdround;		/* rounding table */
static int *pcdL_y_tab;		/* => table for L to caley y conversion */
static int *pcdC2_r_tab;	/* => table for C2 to R conversion */
static int *pcdC1_b_tab;	/* => table for C1 to B conversion */
static int *pcdC2_g_tab;	/* => table for C2 to G conversion */
static int *pcdC1_g_tab;	/* => table for C1 to G conversion */


/*
 * Initialize for colorspace conversion.
 */

void pcd_ycc_rgb_init(void)
{
	int i;

	CURRFUNC("pcd_ycc_rgb_init");
	pcdlimit = 256 + (byte *) lmalloc(3 * 256 * sizeof(byte));

	for (i = -256; i < 512; i++) {
		if (i < 0)
			pcdlimit[i] = 0;
		else if (i > 255)
			pcdlimit[i] = 255;
		else
			pcdlimit[i] = i;
	}

	pcdround = &roundtab[256];
	pcdL_y_tab = (int *) lmalloc(256 * sizeof(int));
	pcdC2_r_tab = (int *) lmalloc(256 * sizeof(int));
	pcdC1_b_tab = (int *) lmalloc(256 * sizeof(int));
	pcdC2_g_tab = (int *) lmalloc(256 * sizeof(int));
	pcdC1_g_tab = (int *) lmalloc(256 * sizeof(int));

	for (i = 0; i < 256; i++) {
		int C2, C1;
		/* PCD is supposed to be variation on CCIR 601-1 YCrCb
		 * It seems that the L channel 100% (diffuse reflection)
		 * is set at 182 (although images seem to exceed this value),
		 * the C1 zero at 156, and the C2 zero at 137.
		 * The Conversion to RGB is supposed to be:
		 * R = L + C2
		 * G = L - 0.194 C1 - 0.509 C2
		 * B = L + C1
		 * 
		 * The primaries are supposed to be CCIR 709 [HDTV], which
		 * are slightly different to the normal SMPTE-C RGB primaries.
		 * The gamma is suposed to be CCIR 709 - that is linear below
		 * 0.018, and exp 1/0.45 above, rather than the SMPTE-C exp 2.2.
		 *
		 * exp 2.05 is used as an approximation.
		 *
		 * The following conversion scales RGB by 255/182 to make sure
		 * that maximum L gives 255,255,255. This probably causes some
		 * clipping. No correction is currently made for CCIR 709 ->
		 * SMPTE-C primaries.
		 */
		C1 = (i - 156);
		C2 = (i - 137);
#define MAXL 182.0
		pcdL_y_tab[i] = (int) (FIX(255.0 / MAXL) * i + ONE_HALF);
		pcdC2_r_tab[i] = (int) (FIX(255.0 / MAXL * 1.0) * C2);
		pcdC1_b_tab[i] = (int) (FIX(255.0 / MAXL * 1.0) * C1);
		pcdC2_g_tab[i] = (int) (-FIX(255.0 / MAXL * 0.509) * C2);
		pcdC1_g_tab[i] = (int) (-FIX(255.0 / MAXL * 0.194) * C1);
	}
}


/*
 * Convert some rows of samples to the output colorspace.
 * (do rotation at the same time if needed)
 */

void pcd_ycc_to_rgb(pcdHeader * hp, Image * image)
{
	byte *Lp, *C1p, *C2p;
	unsigned char *odat;
	int x, y, w, h;

	CURRFUNC("pcd_ycc_to_rgb");

	/* Allocate some output data area */
	odat = (unsigned char *) lmalloc(image->width * image->height * 3);

	if (pcdlimit == NULL)
		pcd_ycc_rgb_init();

	/* We do rotation in blocks to minimize */
	/* virtual memory thrashing on big images. */
#define RBLOCK 128		/* rotate blocking */

	Lp = hp->Lp;
	C1p = hp->C1p;
	C2p = hp->C2p;
	/* Image orientation */
	switch (hp->rotate) {
	case PCD_NO_ROTATE:
		w = image->width;
		h = image->height;
		rot_block(Lp, C1p, C2p, 1, w, odat, w * 3, w, h);
		break;
	case PCD_ACLOCK_ROTATE:
		w = image->height;
		h = image->width;
		for (y = 0; y < h; y += RBLOCK) {
			int soff, ww, wh;
			wh = h - y;
			if (wh > RBLOCK)
				wh = RBLOCK;
			for (x = 0; x < w; x += RBLOCK) {
				ww = w - x;
				if (ww > RBLOCK)
					ww = RBLOCK;
				soff = h - 1 - y + (x * h);
				rot_block(Lp + soff, C1p + soff, C2p + soff, h, -1, odat + 3 * (x + (y * w)), w * 3, ww, wh);
			}
		}
		image->width = w;
		image->height = h;
		break;
	case PCD_CLOCK_ROTATE:
		w = image->height;
		h = image->width;
		for (y = 0; y < h; y += RBLOCK) {
			int soff, ww, wh;
			wh = h - y;
			if (wh > RBLOCK)
				wh = RBLOCK;
			for (x = 0; x < w; x += RBLOCK) {
				ww = w - x;
				if (ww > RBLOCK)
					ww = RBLOCK;
				soff = (w - 1 - x) * h + y;
				rot_block(Lp + soff, C1p + soff, C2p + soff, -h, 1, odat + 3 * (x + (y * w)), w * 3, ww, wh);
			}
		}
		image->width = w;
		image->height = h;
		break;
	}
	lfree(image->data);
	image->data = odat;
}

/* Rotate and convert a rectangular block */
void rot_block(byte * Lp, byte * C1p, byte * C2p, int hii, int vii, byte * op, int voi, int w, int h)
				/* Input start pointers */
				/* Horizontal/virtical input increments */
					/* Output pointer */
				/* Virtical output increment */
{
	int x, y;
	voi -= w * 3;		/* account for horizontal movement */
	vii -= w * hii;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			int l, c1, c2;
			l = *Lp;
			c1 = *C1p;
			c2 = *C2p;
			l = pcdL_y_tab[l];
			*op = pcdround[(l + pcdC2_r_tab[c2]) >> SCALEBITS];
			*(op + 1) = pcdround[(l + pcdC1_g_tab[c1] + pcdC2_g_tab[c2]) >> SCALEBITS];
			*(op + 2) = pcdround[(l + pcdC1_b_tab[c1]) >> SCALEBITS];
			op += 3;
			Lp += hii;
			C1p += hii;
			C2p += hii;
		}
		op += voi;
		Lp += vii;
		C1p += vii;
		C2p += vii;
	}
}

/* Huffman encoded data functions.  */

/*

   Read the huffman tables which consist of an unsigned byte giving
   the number of entries -1, followed by table entries, each of
   which is a series of 4 unsigned bytes - length, highseq, lowseq,
   and key.

   length is the number of bits of the huffman code -1.
   highseq and lowseq are the huffman code which is msb justified.
   key is the data value corresponding to that code.

   The huffman table is stored into an array huff structures.
   The huff structure holds the length in bits of the
   corresponding code, and the decoded data value.

   If the length of the code is less than the size of the array.
   then the array will contain aliases of that code for every
   combination of the unused bits.  This way the huffman decoding
   becomes a straight table lookup. Note that the array must
   be the size of the longest code.

 */

#define get_byte(dest,hp) \
	{ \
	if ((hp)->bytes_left <= 0) \
		if (pcd_read((hp),PCDBLOCK)) \
			goto data_short; \
	--(hp)->bytes_left; \
	dest = *((hp)->bp++); \
	}

#define EMPTYHUFF (-32768)	/* nonsense value */

static huff *
 gethufftable(pcdHeader * hp, int *size)
					/* Table length return value */
{
	unsigned num, i;
	huff *hufftab = NULL;
	boolean lht = FALSE;	/* Set if large (16 bit) huffman table */

	get_byte(num, hp);
	num++;

	/* Allocate double space to allow for branch nodes */
	hufftab = (huff *) lmalloc(sizeof(huff) * (1 << 12));
	bfill(hufftab, sizeof(huff) * (1 << 12), 0xff);

	for (i = 0; i < num; i++) {
		unsigned int length, codeword, value;
		unsigned int j, k;

		get_byte(length, hp);
		length++;	/* Bit length of code word */
		get_byte(codeword, hp);		/* ms byte of codeword */
		get_byte(value, hp);	/* ls byte of codeword */
		codeword = (codeword << 8) | value;
		get_byte(value, hp);	/* decoded value */

		if (length > 12) {
			if (!lht) {
				int sz;
				/* Switch to large (16 bit) huffman table */
				hufftab = (huff *) lrealloc((byte *)hufftab,
					sizeof(huff) * (1 << 16));
				for (sz = sizeof(huff) * (1 << 12); sz < (sizeof(huff) * (1 << 16)); sz *= 2)
					bcopy((byte *) hufftab, ((byte *) hufftab) + sz, sz);
				lht = TRUE;
			}
			if (length > 16) {
				fprintf(stderr, "pcd: Huffman table corrupted\n");
				goto data_error;
			}
		}
		j = codeword;
		k = codeword + (1 << (16 - length));
		if (!lht) {
			j >>= (16 - 12);
			k >>= (16 - 12);
		}
		for (; j < k; j++) {
			hufftab[j].l = length;
			hufftab[j].v = value;
		}
	}
	*size = lht ? 16 : 12;
	return hufftab;

      data_short:
	fprintf(stderr, "pcd: Short data read in huffman table\n");
      data_error:
	if (hufftab != NULL)
		lfree((byte *) hufftab);
	return NULL;
}


/* We keep the current bits left justified in hp->bits, */
/* and return bits right justified. */

#define BITS_PER_WORD (sizeof(unsigned int)*8)

#define load_byte(hp) \
	{ \
	if ((hp)->bytes_left <= 0) \
		if (pcd_read((hp),PCDBLOCK)) \
			goto data_short; \
	--(hp)->bytes_left; \
	(hp)->bits_left += 8; \
	(hp)->bits |= ((unsigned int)(*((hp)->bp++))) << (BITS_PER_WORD - (hp)->bits_left); \
	}

/* Get (and consume) 1 bit */
#define get1bit(dest, hp) \
	{ \
	if ((hp)->bits_left < 1) \
		load_byte(hp); \
	(dest) = (hp)->bits >> (BITS_PER_WORD - 1); \
	(hp)->bits_left--; \
	(hp)->bits <<= 1; \
	}

/* Get (and consume) nb bits */
#define getbits(dest, hp, nb) \
	{ \
	while ((hp)->bits_left < nb) \
		load_byte(hp); \
	(dest) = (hp)->bits >> (BITS_PER_WORD - nb); \
	(hp)->bits_left -= nb; \
	(hp)->bits <<= nb; \
	}

/* Get nb bits without consuming them */
#define nextbits(dest, hp, nb) \
	{ \
	while ((hp)->bits_left < nb) \
		load_byte(hp); \
	(dest) = (hp)->bits >> (BITS_PER_WORD - nb); \
	}

/* Consume nb bits */
#define skipbits(hp,nb) \
	{ \
	while ((hp)->bits_left < nb) \
		load_byte(hp); \
	(hp)->bits_left -= nb; \
	(hp)->bits <<= nb; \
	}

/* Consume nb bits (assuming previous nextbits( >= nb)) */
#define usedbits(hp,nb) \
	{ \
	(hp)->bits_left -= nb; \
	(hp)->bits <<= nb; \
	}

static int huff_sizes[3];	/* Sizes of huffman tables */
static huff *huffs[3];		/* Pointers to huffman tables */

static int gethuffdata(pcdHeader * hp, int p, int tb, int db, int sf)
				/* Number of planes expected */
				/* Table and Data block offsets from where file is now */
				/* scale factor from final size */
{
	byte *Lp, *C1p, *C2p;
	unsigned int bits;
	unsigned int soff;
	int w, h;		/* width, height */
	int i;

	Lp = hp->Lp;
	C1p = hp->C1p;
	C2p = hp->C2p;
	w = hp->width / sf;
	h = hp->height / sf;

	if (pcdlimit == NULL)
		pcd_ycc_rgb_init();

	soff = pcd_offset(hp);	/* where we are now */
	if (pcd_skip(hp, soff + tb * PCDBLOCK))		/* skip to huffman table(s) */
		goto data_short;

	huffs[0] = huffs[1] = huffs[2] = NULL;
	for (i = 0; i < p; i++)
		if ((huffs[i] = gethufftable(hp, &huff_sizes[i])) == NULL)
			goto data_error;

	if (pcd_skip(hp, soff + db * PCDBLOCK))		/* skip to huffman data */
		goto data_short;

	for (;;) {		/* skip until sync */
		nextbits(bits, hp, 24);
		if (bits == 0xfffffe)
			break;
		usedbits(hp, 24);
	}

	for (;;) {		/* Grab pixel data */
		huff *hufft = 0;
		int huffsz = -1;
		byte *dp = 0;
		int wsf = 0;
		int col = 0;

		nextbits(bits, hp, 24);
		if (bits == 0xfffffe) {		/* Found a sync marker */
			int plane;
			static int pmap[4] =
			{0, 3, 1, 2};
			int wvi;
			int row;

			usedbits(hp, 24);

			getbits(plane, hp, 2);	/* New line/plane header */
			plane = pmap[plane];
			getbits(row, hp, 13);
			getbits(bits, hp, 1);
			if (plane > p) {
				fprintf(stderr, "pcd: Illegal plane number in huffman data\n");
				goto data_error;
			}
			hufft = huffs[plane];
			huffsz = huff_sizes[plane];
			wsf = plane != 0 ? 2 : 1;
			wvi = w / wsf;
			if ((row * wsf) >= h) {
				if ((row * wsf) == h)
					break;
				fprintf(stderr, "pcd: Huffman data exceeds image size\n");
				goto data_error;
			}
			dp = plane == 0 ? Lp : plane == 1 ? C1p : C2p;
			dp += row * wvi;
			col = 0;
			continue;
		}
		/* Not sync, must be another pixel delta */
		if (col >= w) {
			fprintf(stderr, "pcd: Huffman data exceeds image size\n");
			goto data_error;
		}
		if (huffsz == 12) {
			int sh, sum;
			bits >>= (24 - 12);
			sh = hufft[bits].l;
			sum = hufft[bits].v;
			if (sh > 12) {
				fprintf(stderr, "pcd: Unknown huffman data code\n");
				goto data_error;
			}
			usedbits(hp, sh);
			sum += *dp;
			*dp = pcdlimit[sum];
			dp += 1;
			col += wsf;
		} else {	/* huffsz == 16 */
			int sh, sum;
			bits >>= (24 - 16);
			sh = hufft[bits].l;
			sum = hufft[bits].v;
			if (sh > 16) {
				fprintf(stderr, "pcd: Unknown huffman data code\n");
				goto data_error;
			}
			usedbits(hp, sh);
			sum += *dp;
			*dp = pcdlimit[sum];
			dp += 1;
			col += wsf;
		}
	}

	for (i = 0; i < p; i++)
		if (huffs[i] != NULL)
			lfree((byte *) huffs[i]);

	return FALSE;

      data_short:
	fprintf(stderr, "pcd: Short data read in huffman data\n");
      data_error:
	for (i = 0; i < p; i++)
		if (huffs[i])
			if (huffs[i] != NULL)
				lfree((byte *) huffs[i]);
	return TRUE;
}
