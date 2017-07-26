/* options.c:
 *
 * finds which option in an array an argument matches
 *
 * jim frost 10.03.89
 *
 * Copyright 1989 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */

#include "copyright.h"
#include "xli.h"
#include <string.h>
#include <stdlib.h>

/* options array and definitions.  If you add something to this you also
 * need to add its OptionId in options.h.
 */

static OptionArray Options[] =
{

	/* general options */

	{"debug", DBUG, NULL, "\
Turn on synchronous mode for debugging. Dump core on error",},
	{"dumpcore", DUMPCORE, NULL, "\
Dump core immediately on error signal",},
	{"default", DEFAULT, NULL, "\
Set the root background to the default pattern and colors.",},
	{"delay", DELAY, "seconds", "\
Set the automatic advance delay for all images.",},
	{"display", DISPLAY, NULL, "\
Indicate the X display you would like to use.",},
	{"dispgamma", DISPLAYGAMMA, "value", "\
Specify the gamma of the display you are using. The default value is 2.2\n\
A value of between 1.6 and 2.8 is resonable.\n\
If images need brightening or darkening, use the -gamma option"},
	{"fillscreen", FILLSCREEN, NULL, "\
Use the whole screen for displaying an image. The image will be zoomed\n\
so that it just fits the size of the screen. If -onroot is also specified,\n\
it will be zoomed to completely fill the screen.",},
	{"fit", FIT, NULL, "\
Force the image(s) to use the default colormap.",},
	{"fork", FORK, NULL, "\
Background automatically.  Turns on -quiet.",},
	{"fullscreen", FULLSCREEN, NULL, "\
Use the whole screen for displaying an image. The image will be surrounded by\n\
a border if it is smaller than the screen. If -onroot is also specified,\n\
the image will be zoomed so that it just fits the size of the screen.",},
	{"geometry", GEOMETRY, "window_geometry", "\
Specify the size of the display window.  Ignored if -fullscreen or -fillscreeni\n\
is given. If used in conjunction with -onroot, this defines the size of the base\n\
image.",},
	{"goto", GOTO, "image_name", "\
When the end of the list of images is reached, go to the first image in the list\n\
with the target name.",},
	{"help", HELP, "[option ...]", "\
Give help on a particular option or series of options.  If no option is\n\
supplied, a list of available options is given.",},
	{"identify", IDENTIFY, NULL, "\
Identify images rather than displaying them.",},
	{"install", INSTALL, NULL, "\
Force colormap installation.  This option is useful for naive window managers\n\
which do not know how to handle colormap installation, but should be avoided\n\
unless necessary.",},
	{"list", LIST, NULL, "\
List the images along the image path.  Use `xli -path' to see the\n\
current image path.",},
	{"onroot", ONROOT, NULL, "\
Place the image on the root window.  If used in conjunction with -fullscreen,\n\
the image will be zoomed to just fit. If used with -fillscreen, the image will\n\
be zoomed to completely fill the screen. -border, -at, and -center also affect the\n\
results.",},
	{"path", PATH, NULL, "\
Display the image path and default extensions that are loaded from the\n\
.xlirc file.",},
	{"pixmap", PIXMAP, NULL, "\
Force the use of a pixmap as backing store.  This may improve performance but\n\
may not work on memory-limited servers.",},
	{"private", PRIVATE, NULL, "\
Force the use of a private colormap.  This happens automatically if a visual\n\
other than the default is used.  This is the opposite of -fit.",},
	{"quiet", QUIET, NULL, "\
Turn off verbose mode.  This is the default if using -onroot or -windowid.",},
	{"supported", SUPPORTED, NULL, "\
Give a list of the supported image types.",},
	{"verbose", VERBOSE, NULL, "\
Turn on verbose mode.  This is the default if using -view.",},
	{"version", VER_NUM, NULL, "\
Show the version number of this version of xli.",},
	{"view", VIEW, NULL, "\
View an image in a window.  This is the default for all but xsetbg.",},
	{"visual", VISUAL, NULL, "\
Force the use of a particular visual to display an image.  Normally xli\n\
will attempt to pick a visual which is reasonable for the supplied image.",},
	{"windowid", WINDOWID, "window_id", "\
Set the background of a particular window.  This is similar to -onroot and\n\
is useful for servers which use an untagged virtual root.  The window ID\n\
should be supplied as a hexadecimal number, eg 0x40003.",},
	{"cache", CACHE, NULL, "\
Force caching of entire input, useful for interactively twiddling images\n\
loaded from the standard input.",},
	{"delete", DELETE, NULL, "\
Enable deleting images with the 'x' key.",},
	{"focus", FOCUS, NULL, "\
Take keyboard focus when viewing in window.",},

	/* image options */

	{"at", AT, NULL, "\
Load the image onto the base image (if using -merge) or the root window (if\n\
using -onroot) at a specific location.",},
	{"background", BACKGROUND, "color", "\
Set the background pixel color for a monochrome image.  See -foreground and\n\
-invert.",},
	{"border", BORDER, "color", "\
Set the color used for the border around centered, placed or clipped images.",},
	{"brighten", BRIGHT, "percentage", "\
Brighten or darken the image by a percentage.  Values greater than 100 will\n\
brighten the image, values smaller than 100 will darken it.\n\
See also the -gamma option.",},
	{"center", CENTER, NULL, "\
Center the image on the base image (if using -merge) or the root window (if\n\
using -onroot).",},
	{"clip", CLIP, "X,Y,W,H", "\
Clip out the rectangle specified by X,Y,W,H and use that as the image.",},
	{"colordither", COLORDITHER, NULL, "\
Dither the image if the number of colors is reduced. This will be slower,\n\
but will give a better looking result when 256 colors or less are used.",},
	{"cdither", COLORDITHER, NULL, "\
See -colordither.",},
	{"colors", COLORS, "number_of_colors", "\
Specify the maximum number of colors to be used in displaying the image.\n\
Values of 1-32768 are acceptable although low values will not look good.\n\
This is done automatically if the server cannot support the depth of the\n\
image.",},
	{"dither", DITHER, NULL, "\
Dither the image into monochrome.  This happens automatically if sent to\n\
a monochrome display.",},
	{"expand", EXPAND, NULL, "\
Expand the image to TrueColor depth if it is not already of this depth.",},
	{"foreground", FOREGROUND, "color", "\
Set the foreground pixel color for a monochrome image.  See -background and\n\
-invert.",},
	{"gamma", GAMMA, "value", "\
Specify the gamma of the display the image was intended to be displayed\n\
on. Images seem to come in two flavors: 1) linear color images, produced by\n\
ray tracers, scanners etc. These sort of images generally look too dark when\n\
displayed directly to a CRT display. 2) Images that have been processed to\n\
look right on a typical CRT display without any sort of processing. These\n\
images have been 'gamma corrected'. By default, xli assumes that\n\
images have been gamma corrected and need no other processing.\n\
If a linear image is displayed, it will look too dark and a gamma value of\n\
1.0 should be specified, so that xli can correct the image for the\n\
CRT display device.\n\
Some formats (RLE) allow the image gamma to be embedded as a comment in the\n\
file itself, and the -gamma option allows overriding of the file comment.\n\
In general, values smaller than 2.2 will lighten the image, and values\n\
greater than 2.2 will darken the image.\n\
This often works better than the -brighten option.",},
	{"gray", GRAY, NULL, "\
Convert a color image to grayscale.  Also called -grey.",},
	{"grey", GRAY, NULL, "\
See -gray.",},
	{"halftone", HALFTONE, NULL, "\
Dither the image into monochrome using a halftone dither.  This preserves\n\
image detail but blows the image up by sixteen times.",},
	{"idelay", IDELAY, NULL, "\
Set the automatic advance delay for this image.  This overrides -delay\n\
temporarily.",},
	{"invert", INVERT, NULL, "\
Invert a monochrome image.  This is the same as specifying `-foreground black'\n\
and `-background white'.",},
	{"iscale", ISCALE, "scale factor", "\
Scale the image using a fast, image-dependent method, if available.\n\
Positive values make the image smaller, negative values larger.\n\
Specifing `auto' will fast-scale the image to fit on the screen.",},
	{"merge", MERGE, NULL, "\
Merge this image onto the previous image.  When used in conjunction with\n\
-at, -center, and -clip you can generate collages.",},
	{"name", NAME, NULL, "\
Specify that the next argument is to be the name of an image.  This is\n\
useful for loading images whose names look to be options.",},
	{"newoptions", NEWOPTIONS, NULL, "\
Clear the options which propagate to all following images.  This is useful\n\
for turning off image processing options which were specified for previous\n\
images.",},
	{"normalize", NORMALIZE, NULL, "\
Normalize the image.  This expands color coverage to fit the colormap as\n\
closely as possible.  It may have good effects on an image which is too\n\
bright or too dark.",},
	{"rotate", ROTATE, "degrees", "\
Rotate the image by 90, 180, or 270 degrees.",},
	{"smooth", SMOOTH, NULL, "\
Perform a smoothing convolution on the image.  This is useful for making\n\
a zoomed image look less blocky.  Multiple -smooth arguments will run\n\
the smoother multiple times.  This option can be quite slow on large images.",},
	{"title", TITLE, "window_title", "\
Set the title of the window used to display the image.",},
	{"xpm", XPM, "{ m | g4 | g | c }", "\
Select the prefered xpm colour mapping:\n\
(m = mono, g4 = 4 level gray, g = gray, c = color ).",},
	{"xzoom", XZOOM, "percentage", "\
Zoom the image along the X axis by a percentage.  See -zoom.",},
	{"yzoom", YZOOM, "percentage", "\
Zoom the image along the X axis by a percentage.  See -zoom.",},
	{"zoom", ZOOM, NULL, "\
Zoom the image along both axes. Values smaller than 100 will reduce the\n\
size of the image, values greater than 100 will enlarge it.  See also\n\
-xzoom and -yzoom.",},
	{NULL, OPT_NOTOPT, NULL, NULL}
};

static int parse_args(char *as, int sl, char **argv);


OptionId optionNumber(char *arg)
{
	int a, b;

	if ((*arg) != '-')
		return (OPT_NOTOPT);
	for (a = 0; Options[a].name; a++) {
		if (!strncmp(arg + 1, Options[a].name, strlen(arg) - 1)) {
			for (b = a + 1; Options[b].name; b++)
				if (!strncmp(arg + 1, Options[b].name, strlen(arg) - 1))
					return (OPT_SHORTOPT);
			return (Options[a].option_id);
		}
	}
	return (OPT_BADOPT);
}

static void listOptions(void)
{
	int a, width;

	printf("\nThe options are:\n\n");

	width = 0;
	for (a = 0; Options[a].name; a++) {
		width += strlen(Options[a].name) + 2;
		if (width > 78) {
			printf("\n");
			width = strlen(Options[a].name) + 2;
		}
		printf("%s%s", Options[a].name, (Options[a + 1].name ? ", " : "\n\n"));
	}
}

static int helpOnOption(char *option)
{
	int a, foundone;

	if (*option == '-')
		option++;
	foundone = 0;
	for (a = 0; Options[a].name; a++)
		if (!strncmp(Options[a].name, option, strlen(option))) {
			printf("Option: %s\nUsage: xli -%s %s\nDescription:\n%s\n\n",
			       Options[a].name, Options[a].name,
			       (Options[a].args ? Options[a].args : ""),
			       Options[a].description);
			foundone = 1;
		}
	if (!foundone) {
		printf("No option `%s'.\n", option);
	}
	return (foundone);
}

static void literalMindedUser(char *s)
{
	printf("The quotes around %s are unnecessary.  You don't have to be so\n\
literal-minded!\n", s);
}

void help(char *option)
{
	char buf[BUFSIZ];

	/* batch help facility
	 */

	if (option) {
		if (!helpOnOption(option))
			listOptions();
		printf("\
Type `xli -help [option ...]' to get help on a particular option or\n\
`xli -help' to enter the interactive help facility.\n\n");
		return;
	}
	/* interactive help facility
	 */

	printf("\nxli Interactive Help Facility\n\n");
	printf("\
Type `?' for a list of options, or `.' or `quit' to leave the interactive\n\
help facility.\n");
	for (;;) {
		printf("help> ");
		buf[BUFSIZ - 1] = '\0';
		if (fgets(buf, BUFSIZ - 1, stdin) == NULL)
			break;	/* EOF */
		while (buf[strlen(buf) - 1] == '\n')
			buf[strlen(buf) - 1] = '\0';

		/* help keywords
		 */

		if (!strcmp(buf, "")) {
			printf("Type `?' for a list of options\n");
			continue;
		}
		if (!strcmp(buf, "?"));
		else if (!strcmp(buf, "quit") || !strcmp(buf, "."))
			exit(0);
		else if (!strcmp(buf, "`?'"))
			literalMindedUser("the question mark");
		else if (!strcmp(buf, "`quit'")) {
			literalMindedUser("quit");
			exit(0);
		} else if (!strcmp(buf, "`.'")) {
			literalMindedUser("the period");
			exit(0);
		} else if (helpOnOption(buf))
			continue;
		listOptions();
		printf("\
You may get this list again by typing `?' at the `help>' prompt, or leave\n\
the interactive help facility with `.' or `quit'.\n");
	}
}


/*
 * Code to process options and set up options structures.
 */

/* Do general options and return no of argv's advanced */
int doGeneralOption(OptionId opid, char **argv, ImageOptions *persist_ops,
	ImageOptions *image_ops)
{
	int a = 0;
	switch (opid) {
	case ONROOT:
		globals.onroot = 1;
		globals.fit = TRUE;	/* assume -fit */
		break;

	case DBUG:
		globals._Xdebug = TRUE;
		break;

	case DUMPCORE:
		globals._DumpCore = TRUE;
		break;

	case DEFAULT:
		globals.set_default = TRUE;
		break;

	case DELAY:
		if (!argv[++a])
			break;
		persist_ops->delay =
		    image_ops->delay = atoi(argv[a]);
		if (image_ops->delay < 0) {
			printf("Bad argument to -delay\n");
			usage(globals.argv0);
			/* NOTREACHED */
			break;
		}
		break;

	case DISPLAY:
		if (argv[++a])
			globals.dname = argv[a];
		break;

	case DISPLAYGAMMA:
		if (argv[++a])
			globals.display_gamma = atof(argv[a]);
		break;

	case FILLSCREEN:
		globals.fillscreen = TRUE;
		break;

	case FIT:
		globals.fit = TRUE;
		break;

	case FORK:
		globals.do_fork = TRUE;
		/* background processes should be seen but not heard */
		globals.verbose = FALSE;
		break;

	case FULLSCREEN:
		globals.fullscreen = TRUE;
		break;

	case GEOMETRY:
		if (argv[++a])
			globals.user_geometry = argv[a];
		break;

	case GOTO:
		if (argv[++a])
			globals.go_to = argv[a];
		break;

	case HELP:
		if (argv[++a])
			do {
				help(argv[a++]);
			} while (argv[a]);
		else
			help(NULL);
		exit(0);

	case IDENTIFY:
		globals.identify = TRUE;
		break;

	case LIST:
		listImages();
		exit(0);

	case INSTALL:
		globals.install = TRUE;
		break;

	case PATH:
		showPath();
		break;

	case PIXMAP:
		globals.use_pixmap = TRUE;
		break;

	case PRIVATE:
		globals.private_cmap = TRUE;
		break;

	case QUIET:
		globals.verbose = FALSE;
		break;

	case SUPPORTED:
		supportedImageTypes();
		break;

	case VERBOSE:
		globals.verbose = TRUE;
		break;

	case VER_NUM:
		version();
		break;

	case VIEW:
		globals.onroot = FALSE;
		break;

	case VISUAL:
		if (argv[++a])
			globals.visual_class = visualClassFromName(argv[a]);
		break;

	case WINDOWID:
		if (!argv[++a])
			break;
		if (sscanf(argv[a], "0x%x", &globals.dest_window) != 1) {
			printf("Bad argument to -windowid\n");
			usage(globals.argv0);
			/* NOTREACHED */
		}
		globals.onroot = TRUE;	/* this means "on special root" */
		globals.fit = TRUE;	/* assume -fit */
		break;

	case CACHE:
		zforcecache(TRUE);
		break;

	case DELETE:
		globals.delete = TRUE;
		break;

	case FOCUS:
		globals.focus = TRUE;
		break;

	default:
		fprintf(stderr, "strange global option #%d\n", opid);
		exit(-1);
		break;
	}

	return a;
}

/* Do locals and return no of argv's advanced */
int doLocalOption(OptionId opid, char **argv, boolean setpersist,
	ImageOptions *persist_ops, ImageOptions *image_ops)
{
	int a = 0;

	switch (opid) {

	case AT:
		if (!argv[++a])
			break;
		if (sscanf(argv[a], "%d,%d",
			   &image_ops->atx, &image_ops->aty) != 2) {
			printf("Bad argument to -at\n");
			usage(globals.argv0);
			/* NOTREACHED */
		} else
			image_ops->ats = TRUE;
		break;

	case BACKGROUND:
		if (argv[++a])
			image_ops->bg = argv[a];
		break;

	case BORDER:
		if (argv[++a])
			image_ops->border = argv[a];
		if (setpersist)
			persist_ops->border = image_ops->border;
		break;

	case BRIGHT:
		if (argv[++a]) {
			image_ops->bright = atoi(argv[a]);
			if (setpersist)
				persist_ops->bright = image_ops->bright;
		}
		break;

	case GAMMA:
		if (argv[++a]) {
			image_ops->gamma = atof(argv[a]);
			if (setpersist)
				persist_ops->gamma = image_ops->gamma;
		}
		break;

	case GRAY:
		image_ops->gray = 1;
		if (setpersist)
			persist_ops->gray = 1;
		break;

	case CENTER:
		image_ops->center = 1;
		break;

	case CLIP:
		if (!argv[++a])
			break;
		if (sscanf(argv[a], "%d,%d,%d,%d",
			   &image_ops->clipx, &image_ops->clipy,
			   &image_ops->clipw, &image_ops->cliph) != 4) {
			printf("Bad argument to -clip\n");
			usage(globals.argv0);
			/* NOTREACHED */
		}
		break;

	case COLORDITHER:
		image_ops->colordither = 1;
		if (setpersist)
			persist_ops->colordither = 1;
		break;

	case COLORS:
		if (!argv[++a])
			break;
		image_ops->colors = atoi(argv[a]);
		if (image_ops->colors < 2) {
			printf("Argument to -colors is too low (ignored)\n");
			image_ops->colors = 0;
		} else if (image_ops->colors > 65536) {
			printf("Argument to -colors is too high (ignored)\n");
			image_ops->colors = 0;
		}
		if (setpersist)
			persist_ops->colors = image_ops->colors;
		break;

	case DITHER:
		image_ops->dither = 1;
		if (setpersist)
			persist_ops->dither = 1;
		break;

	case EXPAND:
		image_ops->expand = 1;
		if (setpersist)
			persist_ops->expand = 1;
		break;

	case FOREGROUND:
		if (argv[++a])
			image_ops->fg = argv[a];
		break;

	case HALFTONE:
		image_ops->dither = 2;
		if (setpersist)
			persist_ops->dither = 2;
		break;

	case IDELAY:
		if (!argv[++a])
			break;
		image_ops->delay = atoi(argv[a]);
		if (image_ops->delay < 0) {
			printf("Bad argument to -idelay\n");
			usage(globals.argv0);
			/* NOTREACHED */
		}
		break;

	case INVERT:
		image_ops->fg = "white";
		image_ops->bg = "black";
		break;

	case ISCALE:
		if (argv[++a]) {
			if (!strcmp(argv[a], "auto")) {
				image_ops->iscale_auto = TRUE;
			} else {
				image_ops->iscale = atoi(argv[a]);
				image_ops->iscale_auto = FALSE;
			}
			if (setpersist) {
				persist_ops->iscale_auto =
					image_ops->iscale_auto;
			}
		}
		break;

	case MERGE:
		image_ops->merge = 1;
		break;

	case NEWOPTIONS:
		if (setpersist) {
			persist_ops->bright = 0;
			persist_ops->colordither = 0;
			persist_ops->colors = 0;
			persist_ops->delay = -1;
			persist_ops->dither = 0;
			persist_ops->gray = 0;
			persist_ops->gamma = UNSET_GAMMA;
			persist_ops->normalize = 0;
			persist_ops->smooth = 0;
			persist_ops->xzoom = 0;
			persist_ops->yzoom = 0;
		}
		break;

	case NORMALIZE:
		image_ops->normalize = 1;
		if (setpersist)
			persist_ops->normalize = image_ops->normalize;
		break;

	case ROTATE:
		if (!argv[++a])
			break;
		image_ops->rotate = atoi(argv[a]);
		if ((image_ops->rotate % 90) != 0) {
			printf("Argument to -rotate must be a multiple of 90 (ignored)\n");
			image_ops->rotate = 0;
		} else
			while (image_ops->rotate < 0)
				image_ops->rotate += 360;
		break;

	case SMOOTH:
		image_ops->smooth = persist_ops->smooth + 1;
		if (setpersist)
			persist_ops->smooth = image_ops->smooth;
		break;

	case TITLE:
		if (argv[++a])
			image_ops->title = argv[a];
		break;

	case XPM:
		if (argv[++a]) {
			image_ops->xpmkeyc = xpmoption(argv[a]);
			if (image_ops->xpmkeyc != 0 && setpersist)
				persist_ops->xpmkeyc = image_ops->xpmkeyc;
		}
		break;

	case XZOOM:
		if (argv[++a]) {
			if (atoi(argv[a]) < 0) {
				printf("Zoom argument must be positive (ignored).\n");
				break;
			}
			image_ops->xzoom = atoi(argv[a]);
			if (setpersist)
				persist_ops->xzoom = image_ops->xzoom;
		}
		break;

	case YZOOM:
		if (argv[++a]) {
			if (atoi(argv[a]) < 0) {
				printf("Zoom argument must be positive (ignored).\n");
				break;
			}
			image_ops->yzoom = atoi(argv[a]);
			if (setpersist)
				persist_ops->yzoom = image_ops->yzoom;
		}
		break;

	case ZOOM:
		if (argv[++a]) {
			if (!strcmp(argv[a], "auto")) {
                                image_ops->zoom_auto = TRUE;
                        } else {
				if (atoi(argv[a]) < 0) {
					printf("Zoom argument must be positive (ignored).\n");
					break;
				}
				image_ops->xzoom = image_ops->yzoom = atoi(argv[a]);
				image_ops->zoom_auto = FALSE;
			}
			if (setpersist) {
				persist_ops->xzoom = persist_ops->yzoom = image_ops->xzoom;
				persist_ops->zoom_auto = image_ops->zoom_auto;
			}
		}
		break;

	default:
		fprintf(stderr, "strange local option #%d\n", opid);
		exit(-1);
		break;
	}

	return a;
}


/*
 * Code to deal with xli trailing options
 */

#include "xlito.h"
#include "ctype.h"

void read_trail_opt(ImageOptions *image_ops, ZFILE *file, Image *image,
	boolean verbose)
{
	CURRFUNC("read_trail_opt");
	if (!image_ops->done_to) {	/* then try and read trailing options */
		char *pp, sc, buf[TOBUFSZ];
		static char key[] = KEY;
		int nib, ti, klen, tlen;
		char *args;
		char **argv;
		int a;

		image_ops->done_to = TRUE;	/* Tried to read image_ops */
		klen = strlen(key);
		sc = key[0];
		nib = zread(file, buf, TOBUFSZ);
		if (nib == 0)
			goto fail;

		for (pp = buf;; pp++) {
			for (; pp < &buf[nib] && *pp != sc; pp++);
			if (pp > &buf[nib - klen])
				goto fail;
			if (strncmp(key, pp, klen) == 0)
				break;
		}
		/* found the first key string, so move the data down to the bottom */
		nib = &buf[nib] - pp;
		bcopy(pp, buf, nib);
		/* try and fill the rest of the buffer up */
		nib += zread(file, buf, TOBUFSZ - nib);

		/* read the length of the string */
		if (!isdigit(buf[klen]) || !isdigit(buf[klen + 1]) ||
		    !isdigit(buf[klen + 2]) || !isdigit(buf[klen + 3]) || buf[klen + 4] != '"')
			goto fail;
		buf[klen + 4] = '\000';
		tlen = atoi(&buf[klen]);
		if (nib < tlen + 2 * (klen + 5))
			goto fail;	/* not enough space to have read string */
		/* check the second key string */
		if (strncmp(key, &buf[tlen + klen + 10], klen) != 0)
			goto fail;
		ti = klen + 5 + tlen;
		if (buf[ti] != '"' || !isdigit(buf[ti + 1]) ||
		    !isdigit(buf[ti + 2]) || !isdigit(buf[ti + 3]) || !isdigit(buf[ti + 4]))
			goto fail;
		buf[ti + 5] = '\000';
		if (tlen != atoi(&buf[ti + 1]))
			goto fail;
		args = (char *) lmalloc(tlen + 1);
		bcopy(&buf[klen + 5], args, tlen);
		args[tlen] = '\000';

		/* Now we have to parse the argument string into individual arguments */
		nib = parse_args(args, tlen, NULL);
		argv = (char **) lmalloc((nib + 2) * sizeof(char *));
		argv[0] = globals.argv0;	/* copy argv[0] */
		argv[nib + 1] = NULL;	/* mark last pointer null */
		parse_args(args, tlen, argv + 1);	/* parse for real now */
		image_ops->to_argv = argv;	/* stash this away */

		/* Now parse the options */
		for (a = 1; image_ops->to_argv[a] != NULL; a++) {
			OptionId opid;
			opid = optionNumber(image_ops->to_argv[a]);

			if (!isLocalOption(opid)) {
				printf("%s: Garbled or illegal trailing option ignored\n", file->filename);
				continue;
			}
			a += doLocalOption(opid, &image_ops->to_argv[a], FALSE, NULL, image_ops);
		}
		goto notfail;

	      fail:		/* all fails arrive here */
		image_ops->to_argv = NULL;
	      notfail:;
	}
	if (image_ops->to_argv && verbose) {	/* remaind user that there are trailing options */
		int a;
		{
			printf("  Found trailing options '");
			for (a = 1; image_ops->to_argv[a] != NULL; a++) {
				printf("%s%s", a == 1 ? "" : " ", image_ops->to_argv[a]);
			}
			printf("'\n");
		}
	}
}


/* Given a string "as" length "sl", parse the string into separate
 * NULL terminated arguments, with pointers to them placed in "argv"
 * If "argv" is null, parse the string and return the number of arguments
 */
static int parse_args(char *as, int sl, char **argv)
{
#define WSPACE 0		/* parser state */
#define ARG 1
#define QUOTE 2

	char *sp;
	int no;
	int state = WSPACE;

	for (no = 0, sp = as; sp < as + sl; sp++) {
		switch (state) {
		case WSPACE:
			if (isspace(*sp))
				break;
			state = ARG;
			no++;
			if (argv != NULL)
				*argv++ = sp;	/* point to start of string */
			break;
		case ARG:
			if (!isspace(*sp))
				break;
			state = WSPACE;
			if (argv != NULL)
				*sp = '\000';
			break;
		}
	}
	if (argv != NULL && state == ARG)	/* Finish last string */
		*sp = '\000';
	return no;
}

/*
 * visual class to name table and back support
 */

static struct visual_class_name {
	int class;		/* numerical value of class */
	char *name;		/* actual name of class */
} VisualClassName[] = {
	{TrueColor,	"TrueColor"},
	{DirectColor,	"DirectColor"},
	{PseudoColor,	"PseudoColor"},
	{StaticColor,	"StaticColor"},
	{GrayScale,	"GrayScale"},
	{StaticGray,	"StaticGray"},
	{StaticGray,	"StaticGrey"},
	{-1,		(char *) 0}
};

int visualClassFromName(char *name)
{
	int a;
	char *s1, *s2;
	int class = -1;

	for (a = 0; VisualClassName[a].name; a++) {
		for (s1 = VisualClassName[a].name, s2 = name; *s1 && *s2; s1++, s2++)
			if ((isupper(*s1) ? tolower(*s1) : *s1) !=
			    (isupper(*s2) ? tolower(*s2) : *s2))
				break;

		if ((*s1 == '\0') || (*s2 == '\0')) {

			/* check for uniqueness.  we special-case StaticGray because we have two
			 * spellings but they are unique if either is found
			 */

			if ((class != -1) && (class != StaticGray)) {
				fprintf(stderr, "%s does not uniquely describe a visual class (ignored)\n", name);
				return (-1);
			}
			class = VisualClassName[a].class;
		}
	}
	if (class == -1)
		fprintf(stderr, "%s is not a visual class (ignored)\n", name);
	return (class);
}

char *nameOfVisualClass(int class)
{
	int a;

	for (a = 0; VisualClassName[a].name; a++)
		if (VisualClassName[a].class == class)
			return (VisualClassName[a].name);
	return ("[Unknown Visual Class]");
}
