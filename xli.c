/*
 * xli.c:
 *
 * generic image loader for X11
 *
 * smar@reptiles.org 1998/09/30
 *
 * Graeme Gill 93/2/24
 *
 * Based on xloadimage by
 * jim frost 09.27.89
 *
 * Copyright 1989, 1990, 1991 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */

#include "copyright.h"
#include "xli.h"
#include "patchlevel"
#include <signal.h>

GlobalsRec globals;

/*
 * used for the -default option.  this is the root weave bitmap with
 * the bits in the order that xli likes.
 */

#define root_weave_width 4
#define root_weave_height 4
static byte root_weave_bits[] =
{
	0xe0, 0xb0, 0xd0, 0x70
};

#define INIT_IMAGE_OPTS(istr){	\
    istr.name = (char *) 0;	\
    istr.fullname = (char *) 0;	\
    istr.loader_idx = -1;	\
    istr.atx = istr.aty = 0;	\
    istr.ats = FALSE;		\
    istr.border = (char *) 0;	\
    istr.bright = 0;		\
    istr.center = FALSE;	\
    istr.clipx = istr.clipy = 0;\
    istr.clipw = istr.cliph = 0;\
    istr.colordither = FALSE;	\
    istr.colors = 0;		\
    istr.delay = -1;		\
    istr.dither = 0;		\
    istr.gamma = UNSET_GAMMA;	\
    istr.gray = FALSE;		\
    istr.iscale = 0;		\
    istr.iscale_auto = FALSE;	\
    istr.merge = FALSE;		\
    istr.normalize = FALSE;	\
    istr.rotate = 0;		\
    istr.smooth = FALSE;	\
    istr.title = NULL;		\
    istr.xpmkeyc = 0;		\
    istr.xzoom = istr.yzoom = 0;\
    istr.fg = (char *) 0;	\
    istr.bg = (char *) 0;	\
    istr.done_to = 0;	\
    istr.to_argv = 0; }

/* deal with unsigned arithmetic problems */
static int scentre(unsigned tspan, unsigned sspan)
{
	return ((int) tspan - (int) sspan) / 2;
}

int main(int argc, char *argv[])
{
	Image *idisp;
	ImageOptions *images;
	int maximages, nimages, i, first, dir;
	unsigned int winwidth, winheight;
	ImageOptions persist_ops;
	char switchval;

	CURRFUNC("main");

	maximages = INIT_MAXIMAGES;
	images = (ImageOptions *) lmalloc(maximages * sizeof(ImageOptions));

	persist_ops.border = (char *) 0;
	persist_ops.bright = 0;
	persist_ops.colordither = FALSE;
	persist_ops.colors = 0;		/* remaining images */
	persist_ops.delay = -1;
	persist_ops.dither = 0;
	persist_ops.gamma = UNSET_GAMMA;
	persist_ops.normalize = FALSE;
	persist_ops.smooth = FALSE;
	persist_ops.xpmkeyc = 0;	/* none */
	persist_ops.xzoom = 0;
	persist_ops.yzoom = 0;
	persist_ops.zoom_auto = FALSE;
	persist_ops.iscale = 0;
	persist_ops.iscale_auto = FALSE;

	/* set up internal error handlers */
	signal(SIGSEGV, internalError);
	signal(SIGBUS, internalError);
	signal(SIGFPE, internalError);
	signal(SIGILL, internalError);

#if defined(_AIX) && defined(_IBMR2)
	/*
	 * the RS/6000 (AIX 3.1) has a new signal, SIGDANGER, which you get
	 * when memory is exhausted.  since malloc() can overcommit,
	 * it's a good idea to trap this one.
	 */
	signal(SIGDANGER, memoryExhausted);
#endif

	if (argc < 2)
		usage(argv[0]);

	/*
	 * defaults and other initial settings.  some of these depend on what
	 * our name was when invoked.
	 */

	loadPathsAndExts();

	xliDefaultDispinfo(&globals.dinfo);
	globals.argv0 = argv[0];	/* so we can get at this elsewhere */
	globals._Xdebug = FALSE;
	globals._DumpCore = FALSE;
	globals.onroot = FALSE;
	globals.verbose = TRUE;
	if (!strcmp(tail(argv[0]), "xview")) {
		globals.onroot = FALSE;
		globals.verbose = TRUE;
	} else if (!strcmp(tail(argv[0]), "xsetbg")) {
		globals.onroot = TRUE;
		globals.verbose = FALSE;
	}

	/* Get display gamma from the environment if we can */
	{
		char *gstr;

		globals.display_gamma = UNSET_GAMMA;
		if ((gstr = getenv("DISPLAY_GAMMA")) != NULL) {
			globals.display_gamma = atof(gstr);
		}

		/* Ignore silly values */
		if (globals.display_gamma > 20.0 || globals.display_gamma < 0.05)
			globals.display_gamma = DEFAULT_DISPLAY_GAMMA;
	}

	globals.dname = NULL;
	globals.fit = FALSE;
	globals.fillscreen = FALSE;
	globals.fullscreen = FALSE;
	globals.go_to = NULL;
	globals.do_fork = FALSE;
	globals.identify = FALSE;
	globals.install = FALSE;
	globals.private_cmap = FALSE;
	globals.use_pixmap = FALSE;
	globals.set_default = FALSE;
	globals.user_geometry = NULL;
	globals.visual_class = -1;
	winwidth = winheight = 0;

	nimages = 0;
	INIT_IMAGE_OPTS(images[nimages]);
	for (i = 1; i < argc; i++) {
		OptionId opid;

		opid = optionNumber(argv[i]);
		if (opid == OPT_BADOPT) {
			printf("%s: Bad option\n", argv[i]);
			usage(argv[0]);
		}

		if (opid == OPT_SHORTOPT) {
			printf("%s: Not enough characters to identify option\n", argv[i]);
			usage(argv[0]);
		}

		if (opid == OPT_NOTOPT || opid == NAME) {
			if (opid == NAME && argv[++i] == NULL)
				continue;

			if (nimages + 1 >= maximages) {
				maximages *= 2;
				images = (ImageOptions *) lrealloc(
					(byte *) images,
					maximages * sizeof(ImageOptions));
			}

			images[nimages].name = argv[i];
			images[nimages].border = persist_ops.border;
			images[nimages].bright = persist_ops.bright;
			images[nimages].colordither = persist_ops.colordither;
			images[nimages].colors = persist_ops.colors;
			images[nimages].delay = persist_ops.delay;
			images[nimages].dither = persist_ops.dither;
			images[nimages].gamma = persist_ops.gamma;
			images[nimages].normalize = persist_ops.normalize;
			images[nimages].smooth = persist_ops.smooth;
			images[nimages].xpmkeyc = persist_ops.xpmkeyc;
			images[nimages].xzoom = persist_ops.xzoom;
			images[nimages].yzoom = persist_ops.yzoom;
			images[nimages].zoom_auto = persist_ops.zoom_auto;
			images[nimages].iscale_auto = persist_ops.iscale_auto;
			nimages++;
			INIT_IMAGE_OPTS(images[nimages]);
		} else if (isGeneralOption(opid)) {
			i += doGeneralOption(opid, &argv[i], &persist_ops,
				&images[nimages]);
		} else if (isLocalOption(opid)) {
			i += doLocalOption(opid, &argv[i], TRUE, &persist_ops,
				&images[nimages]);
		} else {
			printf("%s: Internal error parsing arguments\n",
				argv[0]);
			exit(1);
		}
	}

	if (globals._DumpCore) {
		signal(SIGSEGV, SIG_DFL);
		signal(SIGBUS, SIG_DFL);
		signal(SIGFPE, SIG_DFL);
		signal(SIGILL, SIG_DFL);
	}

	if (globals.fit && (globals.visual_class != -1)) {
		printf("-fit and -visual options are mutually exclusive (ignoring -visual)\n");
		globals.visual_class = -1;
	}
	if (!nimages && !globals.set_default)
		exit(0);

	if (globals.identify) {	/* identify the named image(s) */
		for (i = 0; i < nimages; i++)
			identifyImage(images[i].name);
		exit(0);
	}

	/* start talking to the display */
	if (!xliOpenDisplay(&globals.dinfo, globals.dname)) {
		fprintf(stderr, "%s: Cannot open display '%s'\n",
			globals.argv0, xliDisplayName(globals.dname));
		exit(1);
	}

	if (globals.do_fork) {
		switch (fork()) {
		case -1:
			perror("fork");
			/* FALLTHRU */
		case 0:
			break;
		default:
			exit(0);
		}
	}

	/* -default: resets colormap and load default root weave */

	if (globals.set_default) {
		byte *old_data;
		ImageOptions ioptions;
		boolean tempverb;

		INIT_IMAGE_OPTS(ioptions);
		idisp = newBitImage(root_weave_width, root_weave_height);
		idisp->gamma = globals.display_gamma;
		old_data = idisp->data;
		idisp->data = root_weave_bits;
		tempverb = globals.verbose;
		globals.verbose = FALSE;
		imageOnRoot(&globals.dinfo, idisp, &ioptions);
		globals.verbose = tempverb;
		idisp->data = old_data;
		freeImage(idisp);
		if (!nimages) {
			xliCloseDisplay(&globals.dinfo);
			exit(0);
		}
	}

	/* load in each named image */
	idisp = 0;
	first = -1;
	dir = 1;
	for (i = 0; i < nimages; i += dir) {
		ImageOptions *io;
		Image *inew, *itmp;

		if (i < 0) {
			dir = 1;
			continue;
		}

		io = &images[i];
		if (io->iscale) {
			io->xzoom = io->yzoom = io->iscale < 0 ? 
				100 << -io->iscale : 100 >> io->iscale;
		}

		if (!(inew = loadImage(io, globals.verbose)))
			continue;

		first = (first < 0);

		if (inew->flags & FLAG_ISCALE)
			io->xzoom = io->yzoom = 0;

		if (io->gamma != UNSET_GAMMA)
			inew->gamma = io->gamma;
		else if (UNSET_GAMMA == inew->gamma)
			defaultgamma(inew);

		/* Process any other options that need doing here */
		if (io->border) {
			xliParseXColor(&globals.dinfo, io->border,
				&io->bordercol);
			xliGammaCorrectXColor(&io->bordercol,
				DEFAULT_DISPLAY_GAMMA);
		}

		/*
		 * if first image and we're putting it on the root window
		 * in fullscreen mode, set zoom factors to something reasonable
		 */

		if (first && ((globals.onroot && globals.fullscreen) ||
				globals.fillscreen) && !io->xzoom &&
				!io->yzoom && !io->center) {
			double wr, hr;

			wr = (double) globals.dinfo.width / inew->width;
			hr = (double) globals.dinfo.height / inew->height;
			io->xzoom = io->yzoom = (((wr < hr) ^
				(globals.onroot && globals.fillscreen)) ?
				wr : hr) * 100 + 0.5;
		}

		itmp = processImage(&globals.dinfo, inew, io);
		if (itmp != inew)
			freeImage(inew);

		inew = itmp;

		if (idisp) {
			if (io->center) {
				io->atx = scentre(idisp->width, inew->width);
				io->aty = scentre(idisp->height, inew->height);
			}
			if (!idisp->title) {
				idisp->title = dupString(inew->title);
				idisp->gamma = inew->gamma;
			}
			itmp = merge(idisp, inew, io->atx, io->aty, io);
			if (idisp != itmp) {
				freeImage(idisp);
				idisp = itmp;
			}
			freeImage(inew);
		}

		/*
		 * else if this is the first image on root
		 * set its position and any border needed
		 */
		if (first && globals.onroot && (winwidth || winheight ||
				io->center || io->ats || globals.fullscreen ||
				globals.fillscreen)) {
			int atx = io->atx, aty = io->aty;

			if (!winwidth)
				winwidth = globals.dinfo.width;
			if (!winheight)
				winheight = globals.dinfo.height;

			if (!images[0].ats) {
				atx = scentre(winwidth, inew->width);
				aty = scentre(winheight, inew->height);
			}
			/* use clip to put border around image */
			itmp = clip(inew, -atx, -aty, winwidth, winheight, io);
			if (itmp != inew) {
				freeImage(inew);
				inew = itmp;
			}
			idisp = inew;
		} else if (!idisp) {
			idisp = inew;
		}

		/* if next image is to be merged onto this one,
		 * then go on to the next image.
		 */

		if (globals.onroot || ((i + 1 < nimages) &&
				(images[i + 1].merge)))
			continue;

		dir = 1;
		switchval = imageInWindow(&globals.dinfo, idisp, io,
			argc, argv);

		switch (switchval) {
		/* window got nuked by someone */
		case '\0':
			xliCloseDisplay(&globals.dinfo);
			exit(1);

		/* user quit */
		case '\003':
		case 'q':
			cleanUpWindow(&globals.dinfo);
			xliCloseDisplay(&globals.dinfo);
			exit(0);

		/* delete current image */
		case 'x':
			/* double-check delete-enable here */
			if (globals.delete && io->fullname) {
				if (unlink(io->fullname) < 0) {
					perror(io->fullname);
				} else {
					fprintf(stderr, "Deleted %s\n",
						io->fullname);
				}
			} else {
				i--;
			}

		/* next image */
		case ' ':
		case 'f':
		case 'n':
			if (i >= (nimages - 1) && globals.go_to != NULL) {
				int j;

				for (j = 0; j < nimages; j++) {
					if (!strcmp(images[j].name,
							globals.go_to)) {
						i = j - 1;
						break;
					}
				}

				if (j >= nimages)
					fprintf(stderr, "Target for -goto %s was not found\n", globals.go_to);
			}
			break;

		/* previous image */
		case 'b':
		case 'p':
			dir = -1;
			break;

		case '.':	/* re-load current image */
			i--;
			break;

		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			/* change gamma same way as a command line option */
			if (UNSET_GAMMA == io->gamma)
				io->gamma = idisp->gamma;

			switch (switchval) {
				int l;
				double jump;

			/* set gamma to default image gamma */
			case '0':	
				io->gamma = DEFAULT_DISPLAY_GAMMA;
				break;

			/* set gamma to 1.0 */
			case '1':	
				io->gamma = 1.0;
				break;

			default:
				l = (switchval - '6');
				if (l >= 0)
					l++;
				if (l > 0)
					jump = 1.1;
				else
					jump = 0.909;
				if (l < 0)
					l = -l;
				for (l--; l > 0; l--)
					jump *= jump;
				io->gamma *= jump;
				break;
			}
			if (globals.verbose)
				printf("Image Gamma now %f\n", io->gamma);
			i--;
			break;

		/* rotations */
		case 'l':
		case 'r':
			switch (switchval) {
			case 'l':
				io->rotate -= 90;
				break;
			case 'r':
				io->rotate += 90;
				break;
			}
			while (io->rotate >= 360)
				io->rotate -= 360;
			while (io->rotate < 0)
				io->rotate += 360;
			if (globals.verbose)
				printf("Image rotation is now %d\n",
					io->rotate);
			i--;
			break;

		case '<':
		case '=':
		case '>':
			switch (switchval) {
			case '<':
				io->iscale += 1;
				break;
	
			case '>':
				io->iscale -= 1;
				break;

			case '=':
				io->iscale = 0;
				break;
			}
			io->xzoom = io->yzoom = 0;
			io->zoom_auto = 0;
			io->iscale_auto = 0;

			if (globals.verbose) {
				printf("Image decoder scaling is now %d\n",
					io->iscale);
			}
			i--;
			break;
		}
		freeImage(idisp);
		idisp = 0;
	}

	if (idisp && globals.onroot)
		imageOnRoot(&globals.dinfo, idisp, &images[nimages - 1]);
	xliCloseDisplay(&globals.dinfo);
	exit(0);
}
