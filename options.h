/* options.h:
 *
 * optionNumber() definitions
 *
 * jim frost 10.03.89
 *
 * Copyright 1989 Jim Frost.  See included file "copyright.h" for complete
 * copyright information.
 */

/* enum with the options in it.  If you add one to this you also have to
 * add its information to Options[] in options.c before it becomes available.
 */

typedef enum option_id {

	/* flags */

	OPT_NOTOPT = 0,
	OPT_BADOPT,
	OPT_SHORTOPT,
	NAME,

	/* general options */

	GENERAL_OPTIONS_START,	/* marker */

	DBUG,
	DUMPCORE,
	DEFAULT,
	DELAY,
	DISPLAY,
	DISPLAYGAMMA,
	FILLSCREEN,
	FIT,
	FORK,
	FULLSCREEN,
	GOTO,
	GEOMETRY,
	HELP,
	IDENTIFY,
	INSTALL,
	LIST,
	ONROOT,
	PATH,
	PIXMAP,
	PRIVATE,
	QUIET,
	SUPPORTED,
	VERBOSE,
	VER_NUM,
	VIEW,
	VISUAL,
	WINDOWID,
	CACHE,
	DELETE,
	FOCUS,

	GENERAL_OPTIONS_END,	/* marker */

	/* local options */

	LOCAL_OPTIONS_START,	/* marker */

	AT,
	BACKGROUND,
	BORDER,
	BRIGHT,
	CENTER,
	CLIP,
	COLORDITHER,
	COLORS,
	DITHER,
	EXPAND,
	FOREGROUND,
	GAMMA,
	GRAY,
	HALFTONE,
	IDELAY,
	INVERT,
	MERGE,
	NEWOPTIONS,
	NORMALIZE,
	ROTATE,
	SMOOTH,
	TITLE,
	XPM,
	XZOOM,
	YZOOM,
	ZOOM,
	ISCALE,

	LOCAL_OPTIONS_END	/* marker */

} OptionId;

typedef struct option_array {
	char *name;		/* name minus preceeding '-' */
	OptionId option_id;
	char *args;		/* arguments this option uses or NULL if none */
	char *description;	/* description of this option */
} OptionArray;

OptionId optionNumber(char *arg);	/* options.c */

#define isGeneralOption(opno) ((((int)(opno) > (int)GENERAL_OPTIONS_START) && \
			      ((int)(opno) < (int)GENERAL_OPTIONS_END))       \
			      ? TRUE : FALSE)

#define isLocalOption(opno) ((((int)(opno) > (int)LOCAL_OPTIONS_START) && \
			    ((int)(opno) < (int)LOCAL_OPTIONS_END))       \
			    ? TRUE : FALSE)
