/* compress.c:
 *
 * compress an index colormap by removing unused or duplicate RGB
 * colors. This implementation uses two passes through the image, 1 to
 * figure out which index values are unused, and another to map the
 * pixel values to the new map.
 *
 * Graeme Gill 93/7/14
 */

#include "xli.h"

void compress_cmap(Image * image, unsigned int verbose)
{
	unsigned char *used, fast[32][32][32];
	unsigned int dmask;	/* Depth mask protection */
	Pixel *map;
	unsigned int next_index;
	Intensity *red = image->rgb.red, *green = image->rgb.green,
	*blue = image->rgb.blue;
	Intensity r, g, b;
	unsigned int x, y, badcount = 0, dupcount = 0, unusedcount = 0;

	CURRFUNC("compress");

	if (!RGBP(image) || image->rgb.compressed)
		return;

	if (verbose) {
		printf("  Compressing colormap...");
		fflush(stdout);
	}
	used = (unsigned char *) lcalloc(sizeof(unsigned char) * depthToColors(image->depth));
	dmask = (1 << image->depth) - 1;	/* Mask any illegal bits for that depth */
	map = (Pixel *) lcalloc(sizeof(Pixel) * depthToColors(image->depth));

	/* init fast duplicate check table */
	for (r = 0; r < 32; r++)
		for (g = 0; g < 32; g++)
			for (b = 0; b < 32; b++)
				fast[r][g][b] = 0;

	/* do pass 1 through the image to check index usage */
	if (image->pixlen == 1) {	/* most usual */
		unsigned char *pixptr = image->data, *pixend;
		pixend = pixptr + (image->height * image->width);
		for (; pixptr < pixend; pixptr++)
			used[(*pixptr) & dmask] = 1;
	} else if (image->pixlen == 2) {
		unsigned char *pixptr = image->data, *pixend;
		pixend = pixptr + (2 * image->height * image->width);
		for (; pixptr < pixend; pixptr += 2) {
			register unsigned long temp;
			temp = (*pixptr << 8) | *(pixptr + 1);
			used[temp & dmask] = 1;
		}
	} else {		/* general case */
		byte *pixptr = image->data;
		for (y = 0; y < image->height; y++)
			for (x = 0; x < image->width; x++) {
				used[memToVal(pixptr, image->pixlen) & dmask] = 1;
				pixptr += image->pixlen;
			}
	}

	/* count the bad pixels */
	for (x = image->rgb.used; x < depthToColors(image->depth); x++)
		if (used[x])
			badcount++;

	/* figure out duplicates and unuseds, and create the new mapping */
	next_index = 0;
	for (x = 0; x < image->rgb.used; x++) {
		if (!used[x]) {
			unusedcount++;
			continue;	/* delete this index */
		}
		/* check for duplicate */
		r = red[x];
		g = green[x];
		b = blue[x];
		if (fast[r >> 11][g >> 11][b >> 11]) {	/* if matches fast check */
			/* then do a linear search */
			for (y = x + 1; y < image->rgb.used; y++) {
				if (r == red[y] && g == green[y] && b == blue[y])
					break;
			}
			if (y < image->rgb.used) {	/* found match */
				map[x] = y;
				dupcount++;
				continue;	/* delete this index */
			}
			fast[r >> 11][g >> 11][b >> 11] = 1;
		}
		/* will map to this index */
		map[x] = next_index;
		next_index++;
	}

	/* change the image pixels */
	if (image->pixlen == 1) {	/* most usual */
		unsigned char *pixptr = image->data, *pixend;
		pixend = pixptr + (image->height * image->width);
		for (; pixptr < pixend; pixptr++)
			*pixptr = map[(*pixptr) & dmask];
	} else if (image->pixlen == 2) {
		unsigned char *pixptr = image->data, *pixend;
		pixend = pixptr + (2 * image->height * image->width);
		for (; pixptr < pixend; pixptr += 2) {
			register unsigned long temp;
			temp = (*pixptr << 8) | *(pixptr + 1);
			temp = map[temp & dmask];
			*pixptr = temp >> 8;
			*(pixptr + 1) = temp;
		}
	} else {		/* general case */
		byte *pixptr = image->data;
		for (y = 0; y < image->height; y++)
			for (x = 0; x < image->width; x++) {
				register unsigned long temp;
				temp = memToVal(pixptr, image->pixlen) & dmask;
				temp = map[temp];
				valToMem(temp, pixptr, image->pixlen);
				pixptr += image->pixlen;
			}
	}

	/* change the colormap */
	for (x = 0; x < image->rgb.used; x++) {
		if (!used[x])
			continue;
		red[map[x]] = red[x];
		green[map[x]] = green[x];
		blue[map[x]] = blue[x];
	}
	image->rgb.used = next_index;

	/* clean up */
	lfree((byte *) map);
	lfree(used);

	if (badcount) {
		if (verbose) {
			printf("%d out-of-range pixels, ", badcount);
		} else {
			fprintf(stderr,
				"Warning: %d out-of-range pixels were seen\n",
				badcount);
		}
	}
	if (verbose) {
		if (!unusedcount && !dupcount)
			printf("no improvment\n");
		else {
			if (dupcount)
				printf("%d duplicate%s and %d unused color%s removed...",
					dupcount, (dupcount == 1 ? "" : "s"),
					unusedcount,
					(unusedcount == 1 ? "" : "s"));
			printf("%d unique color%s\n",
				next_index, (next_index == 1 ? "" : "s"));
		}
	}
	image->rgb.compressed = TRUE;	/* don't do it again */
}
