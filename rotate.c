/* rotate.c
 *
 * rotate an image
 *
 * Contributed by Tom Tatlow (tatlow@dash.enet.dec.com)
 */

#include "copyright.h"
#include "xli.h"

/* rotate_bitmap()
 * converts an old bitmap bit position into a new one
 */
static void rotate_bitmap(int num, int pos, int width, int height,
	int *new_num, int *new_pos)
{
	int slen;		/* Length of source line      */
	int dlen;		/* Length of destination line */
	int sx, sy;
	int dx, dy;

	slen = (width / 8) + (width % 8 ? 1 : 0);
	dlen = (height / 8) + (height % 8 ? 1 : 0);
	sy = num / slen;
	sx = ((num - (sy * slen)) * 8) + pos;
	dx = (height - sy) - 1;
	dy = sx;
	*new_num = (dx / 8) + (dy * dlen);
	*new_pos = dx % 8;
}

/* rotate()
 * rotates an image
 */
Image *rotate(Image * iimage, int rotate, int verbose)
{
	int rot;		/* Actual rotation             */
	Image *simage;		/* Source image                */
	Image *dimage;		/* Destination image           */
	byte *sp;		/* Pointer to source data      */
	byte *dp;		/* Pointer to destination data */
	int slinelen;		/* Length of source line       */
	int dlinelen;		/* Length of destination line  */
	int bit[8];		/* Array of hex values         */
	int x, y;
	int i, b;
	int newi, newb;
	byte **yptr;

	bit[0] = 128;
	bit[1] = 64;
	bit[2] = 32;
	bit[3] = 16;
	bit[4] = 8;
	bit[5] = 4;
	bit[6] = 2;
	bit[7] = 1;

	CURRFUNC("rotate");

	if (verbose) {
		printf("  Rotating image by %d degrees...", rotate);
		fflush(stdout);
	}
	simage = iimage;
	dimage = 0;
	for (rot = 0; rot < rotate; rot += 90) {
		switch (simage->type) {
		case IBITMAP:
			dimage = newBitImage(simage->height, simage->width);
			for (x = 0; x < simage->rgb.used; x++) {
				*(dimage->rgb.red + x) = *(simage->rgb.red + x);
				*(dimage->rgb.green + x) = *(simage->rgb.green + x);
				*(dimage->rgb.blue + x) = *(simage->rgb.blue + x);
			}
			slinelen = (simage->width / 8) + (simage->width % 8 ? 1 : 0);
			sp = simage->data;
			dp = dimage->data;
			for (i = 0; i < (slinelen * simage->height); i++)
				for (b = 0; b < 8; b++)
					if (sp[i] & bit[b]) {
						rotate_bitmap(i, b, simage->width, simage->height, &newi, &newb);
						dp[newi] |= bit[newb];
					}
			break;

		case IRGB:
			dimage = newRGBImage(simage->height, simage->width, simage->depth);
			for (x = 0; x < simage->rgb.used; x++) {
				*(dimage->rgb.red + x) = *(simage->rgb.red + x);
				*(dimage->rgb.green + x) = *(simage->rgb.green + x);
				*(dimage->rgb.blue + x) = *(simage->rgb.blue + x);
			}
			dimage->rgb.used = simage->rgb.used;

			/* build array of y axis ptrs into destination image
			 */

			yptr = (byte **) lmalloc(simage->width * sizeof(char *));
			dlinelen = simage->height * dimage->pixlen;
			for (y = 0; y < simage->width; y++)
				yptr[y] = dimage->data + (y * dlinelen);

			/* rotate
			 */

			sp = simage->data;
			if (simage->pixlen == 1 && dimage->pixlen == 1)		/* most common */
				for (y = 0; y < simage->height; y++)
					for (x = 0; x < simage->width; x++) {
						register unsigned long temp;
						temp = memToVal(sp, 1);
						valToMem(temp, yptr[x] + (simage->height - y - 1), 1);
						sp += 1;
			} else	/* less common */
				for (y = 0; y < simage->height; y++)
					for (x = 0; x < simage->width; x++) {
						register unsigned long temp;
						temp = memToVal(sp, simage->pixlen);
						valToMem(temp,
							 yptr[x] + ((simage->height - y - 1) * dimage->pixlen),
							 dimage->pixlen);
						sp += simage->pixlen;
					}
			lfree((byte *) yptr);
			break;

		case ITRUE:
			if (TRUEP(simage))
				dimage = newTrueImage(simage->height, simage->width);

			/* build array of y axis ptrs into destination image
			 */

			yptr = (byte **) lmalloc(simage->width * sizeof(char *));
			dlinelen = simage->height * dimage->pixlen;
			for (y = 0; y < simage->width; y++)
				yptr[y] = dimage->data + (y * dlinelen);

			/* rotate
			 */

			sp = simage->data;
			if (simage->pixlen == 3 && dimage->pixlen == 3)		/* most common */
				for (y = 0; y < simage->height; y++)
					for (x = 0; x < simage->width; x++) {
						register unsigned long temp;
						temp = memToVal(sp, 3);
						valToMem(temp, yptr[x] + ((simage->height - y - 1) * 3), 3);
						sp += 3;
			} else	/* less common */
				for (y = 0; y < simage->height; y++)
					for (x = 0; x < simage->width; x++) {
						register unsigned long temp;
						temp = memToVal(sp, simage->pixlen);
						valToMem(temp,
							 yptr[x] + ((simage->height - y - 1) * dimage->pixlen),
							 dimage->pixlen);
						sp += simage->pixlen;
					}
			lfree((byte *) yptr);
			break;
		default:
			printf("rotate: Unsupported image type\n");
			exit(1);
		}
		if (simage != iimage)
			freeImage(simage);
		simage = dimage;
	}
	dimage->title = (char *) lmalloc(strlen(iimage->title) + 40);
	sprintf(dimage->title, "%s (rotated by %d degrees)", iimage->title, rot);
	dimage->gamma = iimage->gamma;
	if (verbose)
		printf("done\n");
	return (dimage);
}
