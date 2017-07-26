/* halftone.c:
 *
 * routine for dithering a color image to monochrome based on color
 * intensity.  this is loosely based on an algorithm which barry shein
 * (bzs@std.com) used in his "xf" program.
 *
 * jim frost 07.10.89
 *
 * 16/10/92 - re do intensity tables to be rounded (so that they add up
 *            properly !) - GWG
 *
 * Copyright 1989, 1990 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */

#include "copyright.h"
#include "xli.h"

/* RGB intensity tables.  red is (val * 0.30), green is (val * 0.59), blue
 * is (val * .11), where val is intensity >> 8.  these are used by the
 * colorIntensity() macro in images.h.
 */

unsigned short RedIntensity[256] =
{
	0,      77,     154,    230,    307,    384,    461,    538,
	614,    691,    768,    845,    922,    998,    1075,   1152,
	1229,   1306,   1382,   1459,   1536,   1613,   1690,   1766,
	1843,   1920,   1997,   2074,   2150,   2227,   2304,   2381,
	2458,   2534,   2611,   2688,   2765,   2842,   2918,   2995,
	3072,   3149,   3226,   3302,   3379,   3456,   3533,   3610,
	3686,   3763,   3840,   3917,   3994,   4070,   4147,   4224,
	4301,   4378,   4454,   4531,   4608,   4685,   4762,   4838,
	4915,   4992,   5069,   5146,   5222,   5299,   5376,   5453,
	5530,   5606,   5683,   5760,   5837,   5914,   5990,   6067,
	6144,   6221,   6298,   6374,   6451,   6528,   6605,   6682,
	6758,   6835,   6912,   6989,   7066,   7142,   7219,   7296,
	7373,   7450,   7526,   7603,   7680,   7757,   7834,   7910,
	7987,   8064,   8141,   8218,   8294,   8371,   8448,   8525,
	8602,   8678,   8755,   8832,   8909,   8986,   9062,   9139,
	9216,   9293,   9370,   9446,   9523,   9600,   9677,   9754,
	9830,   9907,   9984,   10061,  10138,  10214,  10291,  10368,
	10445,  10522,  10598,  10675,  10752,  10829,  10906,  10982,
	11059,  11136,  11213,  11290,  11366,  11443,  11520,  11597,
	11674,  11750,  11827,  11904,  11981,  12058,  12134,  12211,
	12288,  12365,  12442,  12518,  12595,  12672,  12749,  12826,
	12902,  12979,  13056,  13133,  13210,  13286,  13363,  13440,
	13517,  13594,  13670,  13747,  13824,  13901,  13978,  14054,
	14131,  14208,  14285,  14362,  14438,  14515,  14592,  14669,
	14746,  14822,  14899,  14976,  15053,  15130,  15206,  15283,
	15360,  15437,  15514,  15590,  15667,  15744,  15821,  15898,
	15974,  16051,  16128,  16205,  16282,  16358,  16435,  16512,
	16589,  16666,  16742,  16819,  16896,  16973,  17050,  17126,
	17203,  17280,  17357,  17434,  17510,  17587,  17664,  17741,
	17818,  17894,  17971,  18048,  18125,  18202,  18278,  18355,
	18432,  18509,  18586,  18662,  18739,  18816,  18893,  18970,
	19046,  19123,  19200,  19277,  19354,  19430,  19507,  19584
};

unsigned short GreenIntensity[256] =
{
	0,      151,    302,    453,    604,    755,    906,    1057,
	1208,   1359,   1510,   1661,   1812,   1964,   2115,   2266,
	2417,   2568,   2719,   2870,   3021,   3172,   3323,   3474,
	3625,   3776,   3927,   4078,   4229,   4380,   4531,   4682,
	4833,   4984,   5135,   5286,   5437,   5588,   5740,   5891,
	6042,   6193,   6344,   6495,   6646,   6797,   6948,   7099,
	7250,   7401,   7552,   7703,   7854,   8005,   8156,   8307,
	8458,   8609,   8760,   8911,   9062,   9213,   9364,   9516,
	9667,   9818,   9969,   10120,  10271,  10422,  10573,  10724,
	10875,  11026,  11177,  11328,  11479,  11630,  11781,  11932,
	12083,  12234,  12385,  12536,  12687,  12838,  12989,  13140,
	13292,  13443,  13594,  13745,  13896,  14047,  14198,  14349,
	14500,  14651,  14802,  14953,  15104,  15255,  15406,  15557,
	15708,  15859,  16010,  16161,  16312,  16463,  16614,  16765,
	16916,  17068,  17219,  17370,  17521,  17672,  17823,  17974,
	18125,  18276,  18427,  18578,  18729,  18880,  19031,  19182,
	19333,  19484,  19635,  19786,  19937,  20088,  20239,  20390,
	20541,  20692,  20844,  20995,  21146,  21297,  21448,  21599,
	21750,  21901,  22052,  22203,  22354,  22505,  22656,  22807,
	22958,  23109,  23260,  23411,  23562,  23713,  23864,  24015,
	24166,  24317,  24468,  24620,  24771,  24922,  25073,  25224,
	25375,  25526,  25677,  25828,  25979,  26130,  26281,  26432,
	26583,  26734,  26885,  27036,  27187,  27338,  27489,  27640,
	27791,  27942,  28093,  28244,  28396,  28547,  28698,  28849,
	29000,  29151,  29302,  29453,  29604,  29755,  29906,  30057,
	30208,  30359,  30510,  30661,  30812,  30963,  31114,  31265,
	31416,  31567,  31718,  31869,  32020,  32172,  32323,  32474,
	32625,  32776,  32927,  33078,  33229,  33380,  33531,  33682,
	33833,  33984,  34135,  34286,  34437,  34588,  34739,  34890,
	35041,  35192,  35343,  35494,  35645,  35796,  35948,  36099,
	36250,  36401,  36552,  36703,  36854,  37005,  37156,  37307,
	37458,  37609,  37760,  37911,  38062,  38213,  38364,  38515
};

unsigned short BlueIntensity[256] =
{
	0,      28,     56,     84,     113,    141,    169,    197,
	225,    253,    282,    310,    338,    366,    394,    422,
	451,    479,    507,    535,    563,    591,    620,    648,
	676,    704,    732,    760,    788,    817,    845,    873,
	901,    929,    957,    986,    1014,   1042,   1070,   1098,
	1126,   1155,   1183,   1211,   1239,   1267,   1295,   1324,
	1352,   1380,   1408,   1436,   1464,   1492,   1521,   1549,
	1577,   1605,   1633,   1661,   1690,   1718,   1746,   1774,
	1802,   1830,   1859,   1887,   1915,   1943,   1971,   1999,
	2028,   2056,   2084,   2112,   2140,   2168,   2196,   2225,
	2253,   2281,   2309,   2337,   2365,   2394,   2422,   2450,
	2478,   2506,   2534,   2563,   2591,   2619,   2647,   2675,
	2703,   2732,   2760,   2788,   2816,   2844,   2872,   2900,
	2929,   2957,   2985,   3013,   3041,   3069,   3098,   3126,
	3154,   3182,   3210,   3238,   3267,   3295,   3323,   3351,
	3379,   3407,   3436,   3464,   3492,   3520,   3548,   3576,
	3604,   3633,   3661,   3689,   3717,   3745,   3773,   3802,
	3830,   3858,   3886,   3914,   3942,   3971,   3999,   4027,
	4055,   4083,   4111,   4140,   4168,   4196,   4224,   4252,
	4280,   4308,   4337,   4365,   4393,   4421,   4449,   4477,
	4506,   4534,   4562,   4590,   4618,   4646,   4675,   4703,
	4731,   4759,   4787,   4815,   4844,   4872,   4900,   4928,
	4956,   4984,   5012,   5041,   5069,   5097,   5125,   5153,
	5181,   5210,   5238,   5266,   5294,   5322,   5350,   5379,
	5407,   5435,   5463,   5491,   5519,   5548,   5576,   5604,
	5632,   5660,   5688,   5716,   5745,   5773,   5801,   5829,
	5857,   5885,   5914,   5942,   5970,   5998,   6026,   6054,
	6083,   6111,   6139,   6167,   6195,   6223,   6252,   6280,
	6308,   6336,   6364,   6392,   6420,   6449,   6477,   6505,
	6533,   6561,   6589,   6618,   6646,   6674,   6702,   6730,
	6758,   6787,   6815,   6843,   6871,   6899,   6927,   6956,
	6984,   7012,   7040,   7068,   7096,   7124,   7153,   7181
};

/* 4x4 arrays used for dithering, arranged by nybble
 */

#define GRAYS    17		/* ((4 * 4) + 1) patterns for a good dither */
#define GRAYSTEP ((unsigned long)(65536 / GRAYS))

static byte DitherBits[GRAYS][4] =
{
	{0xf, 0xf, 0xf, 0xf},
	{0xe, 0xf, 0xf, 0xf},
	{0xe, 0xf, 0xb, 0xf},
	{0xa, 0xf, 0xb, 0xf},
	{0xa, 0xf, 0xa, 0xf},
	{0xa, 0xd, 0xa, 0xf},
	{0xa, 0xd, 0xa, 0x7},
	{0xa, 0x5, 0xa, 0x7},
	{0xa, 0x5, 0xa, 0x5},
	{0x8, 0x5, 0xa, 0x5},
	{0x8, 0x5, 0x2, 0x5},
	{0x0, 0x5, 0x2, 0x5},
	{0x0, 0x5, 0x0, 0x5},
	{0x0, 0x4, 0x0, 0x5},
	{0x0, 0x4, 0x0, 0x1},
	{0x0, 0x0, 0x0, 0x1},
	{0x0, 0x0, 0x0, 0x0}
};

/* simple dithering algorithm, really optimized for the 4x4 array
 */

Image *halftone(Image * cimage, unsigned int verbose)
{
	Image *image;
	unsigned char *sp, *dp, *dp2;	/* data pointers */
	unsigned int dindex;	/* index into dither array */
	unsigned int spl;	/* source pixel length in bytes */
	unsigned int dll;	/* destination line length in bytes */
	Pixel color;		/* pixel color */
	unsigned int *index;	/* index into dither array for a given pixel */
	unsigned int a, x, y;	/* random counters */

	CURRFUNC("halftone");
	if (BITMAPP(cimage))
		return (NULL);

	/* set up
	 */

	if (GAMMA_NOT_EQUAL(cimage->gamma, 1.0))
		gammacorrect(cimage, 1.0, verbose);

	if (verbose) {
		printf("  Halftoning image...");
		fflush(stdout);
	}
	image = newBitImage(cimage->width * 4, cimage->height * 4);
	if (cimage->title) {
		image->title = (char *) lmalloc(strlen(cimage->title) + 13);
		sprintf(image->title, "%s (halftoned)", cimage->title);
	}
	image->gamma = cimage->gamma;
	spl = cimage->pixlen;
	dll = (image->width / 8) + (image->width % 8 ? 1 : 0);

	/* if the number of possible pixels isn't very large, build an array
	 * which we index by the pixel value to find the dither array index
	 * by color brightness.  we do this in advance so we don't have to do
	 * it for each pixel.  things will break if a pixel value is greater
	 * than (1 << depth), which is bogus anyway.  this calculation is done
	 * on a per-pixel basis if the colormap is too big.
	 */

	if (RGBP(cimage) && (cimage->depth <= 16)) {
		index = (unsigned int *) lmalloc(sizeof(unsigned int) * cimage->rgb.used);
		for (x = 0; x < cimage->rgb.used; x++) {
			*(index + x) =
			    ((unsigned long) colorIntensity(*(cimage->rgb.red + x),
						*(cimage->rgb.green + x),
				    *(cimage->rgb.blue + x))) / GRAYSTEP;
			if (*(index + x) >= GRAYS)	/* rounding errors can do this */
				*(index + x) = GRAYS - 1;
		}
	} else
		index = NULL;

	/* dither each pixel */

	sp = cimage->data;
	dp = image->data;
	for (y = 0; y < cimage->height; y++) {
		for (x = 0; x < cimage->width; x++) {
			dp2 = dp + (x >> 1);
			color = memToVal(sp, spl);
			if (RGBP(cimage)) {
				if (index)
					dindex = *(index + color);
				else {
					dindex =
						((unsigned long) colorIntensity(cimage->rgb.red[color], cimage->rgb.green[color], cimage->rgb.blue[color])) / GRAYSTEP;
				}
			} else {
				dindex =
					((unsigned long) colorIntensity((TRUE_RED(color) << 8), (TRUE_GREEN(color) << 8), (TRUE_BLUE(color) << 8))) / GRAYSTEP;
			}
			if (dindex >= GRAYS)	/* rounding errors can do this */
				dindex = GRAYS - 1;

			/*
			 * loop for the four Y bits in the dither pattern,
			 * putting all four X bits in at once.  if you
			 * think this would be hard to change to be an
			 * NxN dithering array, you're right, since we're
			 * banking on the fact that we need only shift the
			 * mask based on whether x is odd or not.  an 8x8
			 * array wouldn't even need that, but blowing an
			 * image up by 64x is probably not a feature.
			 */

			if (x & 1)
				for (a = 0; a < 4; a++, dp2 += dll)
					*dp2 |= DitherBits[dindex][a];
			else
				for (a = 0; a < 4; a++, dp2 += dll)
					*dp2 |= (DitherBits[dindex][a] << 4);
			sp += spl;
		}
		dp += (dll << 2);	/* (dll * 4) but I like shifts */
	}
	if (RGBP(cimage) && (cimage->depth <= 16))
		lfree((byte *) index);
	if (verbose)
		printf("done\n");
	return (image);
}
