/* #ident	"@(#)x11:contrib/clients/xloadimage/rlelib.c 6.15 93/07/23 Labtam" */
/*
 *	Utah RLE Toolkit library routines.
 * 
 * 	Read image support only.
 * 
 * 	Cobbled from Utah RLE include and library source files.
 * 
 * 	By Graeme Gill
 * 	30/5/90
 *
 */

#include "xli.h"
#include <ctype.h>
#include "rle.h"

#undef  DEBUG

#ifdef DEBUG
# define debug(xx)	fprintf(stderr,xx)
#else
# define debug(xx)
#endif

/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/* 
 * Runsv.h - Definitions for Run Length Encoding.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Mon Aug  9 1982
 * Copyright (c) 1982 Spencer W. Thomas
 */

#ifndef XTNDRUNSV
#define XTNDRUNSV

/* 
 * Opcode definitions
 */

#define     LONG                0x40
#define	    RSkipLinesOp	1
#define	    RSetColorOp		2
#define	    RSkipPixelsOp	3
#define	    RByteDataOp		5
#define	    RRunDataOp		6
#define	    REOFOp		7

#define     H_CLEARFIRST        0x1	/* clear framebuffer flag */
#define	    H_NO_BACKGROUND	0x2	/* if set, no bg color supplied */
#define	    H_ALPHA		0x4   /* if set, alpha channel (-1) present */
#define	    H_COMMENT		0x8	/* if set, comments present */

struct XtndRsetup
{
    short   h_xpos,
            h_ypos,
            h_xlen,
            h_ylen;
    char    h_flags,
            h_ncolors,
	    h_pixelbits,
	    h_ncmap,
	    h_cmaplen;
};

/* "Old" RLE format magic numbers */
#define	    RMAGIC	('R' << 8)	/* top half of magic number */
#define	    WMAGIC	('W' << 8)	/* black&white rle image */

#define	    XtndRMAGIC	((short)0xcc52)	/* RLE file magic number */

#endif /* XTNDRUNSV */

/*  "svfb.h" */
/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/* 
 * svfb.h - Definitions and a few global variables for svfb.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Mon Aug  9 1982
 * Copyright (c) 1982 Spencer W. Thomas
 */

/* 
 * States for run detection
 */
#define	DATA	0
#define	RUN2	1
#define RUN3	2
#define	RUN4	3
#define	INRUN	-1

/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 *
 *  Modified at BRL 16-May-88 by Mike Muuss to avoid Alliant STDC desire
 *  to have all "void" functions so declared.
 */
/* 
 * svfb_global.c - Global variable initialization for svfb routines.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Thu Apr 25 1985
 * Copyright (c) 1985,1986 Spencer W. Thomas
 */

static int sv_bg_color[3] = { 0, 0, 0 };

struct sv_globals sv_globals = {
    RUN_DISPATCH,		/* dispatch value */
    3,				/* 3 colors */
    sv_bg_color,		/* background color */
    0,				/* (alpha) if 1, save alpha channel */
    2,				/* (background) 0->just save pixels, */
				/* 1->overlay, 2->clear to bg first */
    0, 511,			/* (xmin, xmax) X bounds to save */
    0, 479,			/* (ymin, ymax) Y bounds to save */
    0,				/* ncmap (if != 0, save color map) */
    8,				/* cmaplen (log2 of length of color map) */
    NULL,			/* pointer to color map */
    NULL,			/* pointer to comment strings */
    NULL,			/* output file */
    { 7 }			/* RGB channels only */
    /* Can't initialize the union */
};

/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 *
 *  Modified at BRL 16-May-88 by Mike Muuss to avoid Alliant STDC desire
 *  to have all "void" functions so declared.
 */
/* 
 * Runsv.c - General purpose Run Length Encoding for svfb.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Mon Aug  9 1982
 * Copyright (c) 1982,1986 Spencer W. Thomas
 */

/* THIS IS WAY OUT OF DATE.  See rle.5.
 * The output file format is:
 * 
 * Word 0:	A "magic" number.  The top byte of the word contains
 *		the letter 'R' or the letter 'W'.  'W' indicates that
 *		only black and white information was saved.  The bottom
 *		byte is one of the following:
 *	' ':	Means a straight "box" save, -S flag was given.
 *	'B':	Image saved with background color, clear screen to
 *		background before restoring image.
 *	'O':	Image saved in overlay mode.
 * 
 * Words 1-6:	The structure
 * {   short   xpos,			* Lower left corner
 *             ypos,
 *             xsize,			* Size of saved box
 *             ysize;
 *     char    rgb[3];			* Background color
 *     char    map;			* flag for map presence
 * }
 * 
 * If the map flag is non-zero, then the color map will follow as 
 * 3*256 16 bit words, first the red map, then the green map, and
 * finally the blue map.
 * 
 * Following the setup information is the Run Length Encoded image.
 * Each instruction consists of a 4-bit opcode, a 12-bit datum and
 * possibly one or more following words (all words are 16 bits).  The
 * instruction opcodes are:
 * 
 * SkipLines (1):   The bottom 10 bits are an unsigned number to be added to
 *		    current Y position.
 * 
 * SetColor (2):    The datum indicates which color is to be loaded with
 * 		    the data described by the following ByteData and
 * 		    RunData instructions.  0->red, 1->green, 2->blue.  The
 * 		    operation also resets the X position to the initial
 * 		    X (i.e. a carriage return operation is performed).
 * 
 * SkipPixels (3):  The bottom 10 bits are an unsigned number to be
 * 		    added to the current X position.
 * 
 * ByteData (5):    The datum is one less than the number of bytes of
 * 		    color data following.  If the number of bytes is
 * 		    odd, a filler byte will be appended to the end of
 * 		    the byte string to make an integral number of 16-bit
 * 		    words.  The bytes are in PDP-11 order.  The X
 * 		    position is incremented to follow the last byte of
 * 		    data.
 * 
 * RunData (6):	    The datum is one less than the run length.  The
 * 		    following word contains (in its lower 8 bits) the
 * 		    color of the run.  The X position is incremented to
 * 		    follow the last byte in the run.
 */

#define UPPER 255			/* anything bigger ain't a byte */

/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/* 
 * buildmap.c - Build a color map from the RLE file color map.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Sat Jan 24 1987
 * Copyright (c) 1987, University of Utah
 */

/*****************************************************************
 * TAG( buildmap )
 * 
 * Returns a color map that can easily be used to map the pixel values in
 * an RLE file.  Map is built from the color map in the input file.
 * Inputs:
 * 	globals:	sv_globals structure containing color map.
 *	minmap:		Minimum number of channels in output map.
 *	gamma:		Adjust color map for this image gamma value
 *			(1.0 means no adjustment).
 * Outputs:
 * 	Returns an array of pointers to arrays of rle_pixels.  The array
 *	of pointers contains max(sv_ncolors, sv_ncmap) elements, each 
 *	array of pixels contains 2^sv_cmaplen elements.  The pixel arrays
 *	should be considered read-only.
 * Assumptions:
 * 	[None]
 * Algorithm:
 *	Ensure that there are at least sv_ncolors rows in the map, and
 *	that each has at least 256 elements in it (largest map that can
 *	be addressed by an rle_pixel).
 *      Record the size of cmap in cmap[-1] so that the memory can be freed
 *	later.
 */
rle_pixel **
buildmap(struct sv_globals *globals, int minmap, double gamma)
{
    rle_pixel ** cmap, * gammap;
    register int i, j;
    int maplen, cmaplen, ncmap, nmap;

    if ( globals->sv_ncmap == 0 )	/* make identity map */
    {
	nmap = (minmap < globals->sv_ncolors) ? globals->sv_ncolors : minmap;
	cmap = (rle_pixel **)lcalloc((nmap +1) * sizeof(rle_pixel *));
	cmap[0] = (rle_pixel *)(nmap +1);
	cmap++;	/* keep size in cmap[-1] */
	cmap[0] = (rle_pixel *)lmalloc( 256 * sizeof(rle_pixel) );
	for ( i = 0; i < 256; i++ )
	    cmap[0][i] = i;
	for ( i = 1; i < nmap; i++ )
	    cmap[i] = cmap[0];
	maplen = 256;
	ncmap = 1;		/* number of unique rows */
    }
    else			/* make map from globals */
    {
	/* Map is at least 256 long */
	cmaplen = (1 << globals->sv_cmaplen);
	if ( cmaplen < 256 )
	    maplen = 256;
	else
	    maplen = cmaplen;

	if ( globals->sv_ncmap == 1 )	/* make "b&w" map */
	{
	    nmap = (minmap < globals->sv_ncolors) ?
		globals->sv_ncolors : minmap;
	    cmap = (rle_pixel **)lcalloc((nmap+1) * sizeof(rle_pixel *));
	    cmap[0] = (rle_pixel *)(nmap+1);
	    cmap++;	/* keep size in cmap[-1] */
	    cmap[0] = (rle_pixel *)lmalloc( maplen * sizeof(rle_pixel) );
	    for ( i = 0; i < maplen; i++ )
		if ( i < cmaplen )
		    cmap[0][i] = globals->sv_cmap[i] >> 8;
		else
		    cmap[0][i] = i;
	    for ( i = 1; i < nmap; i++ )
		cmap[i] = cmap[0];
	    ncmap = 1;
	}
	else if ( globals->sv_ncolors <= globals->sv_ncmap )
	{
	    nmap = (minmap < globals->sv_ncmap) ? globals->sv_ncmap : minmap;
	    cmap = (rle_pixel **)lcalloc((nmap+1) * sizeof(rle_pixel *));
	    cmap[0] = (rle_pixel *)(nmap+1);
	    cmap++;	/* keep size in cmap[-1] */
	    for ( j = 0; j < globals->sv_ncmap; j++ )
	    {
		cmap[j] = (rle_pixel *)lmalloc( maplen * sizeof(rle_pixel) );
		for ( i = 0; i < maplen; i++ )
		    if ( i < cmaplen )
			cmap[j][i] = globals->sv_cmap[j*cmaplen + i] >> 8;
		    else
			cmap[j][i] = i;
	    }
	    for ( i = j, j--; i < nmap; i++ )
		cmap[i] = cmap[j];
	    ncmap = globals->sv_ncmap;
	}
	else			/* ncolors > ncmap */
	{
	    nmap = (minmap < globals->sv_ncolors) ?
		globals->sv_ncolors : minmap;
	    cmap = (rle_pixel **)lcalloc((nmap+1) * sizeof(rle_pixel *));
	    cmap[0] = (rle_pixel *)(nmap+1);
	    cmap++;	/* keep size in cmap[-1] */
	    for ( j = 0; j < globals->sv_ncmap; j++ )
	    {
		cmap[j] = (rle_pixel *)lmalloc( maplen * sizeof(rle_pixel) );
		for ( i = 0; i < maplen; i++ )
		    if ( i < cmaplen )
			cmap[j][i] = globals->sv_cmap[j*cmaplen + i] >> 8;
		    else
			cmap[j][i] = i;
	    }
	    for( i = j, j--; i < nmap; i++ )
		cmap[i] = cmap[j];
	    ncmap = globals->sv_ncmap;
	}
    }
	    
    /* Gamma compensate if requested */
    if ( gamma != 1.0 )
    {
	gammap = (rle_pixel *)lmalloc( 256 * sizeof(rle_pixel) );
	for ( i = 0; i < 256; i++ )
		{
#ifdef BYTEBUG
		int byteb1;
		byteb1 = (int)(0.5 + 255.0 * pow( i / 255.0, gamma ));
		gammap[i] = byteb1;
#else
	    gammap[i] = (int)(0.5 + 255.0 * pow( i / 255.0, gamma ));
#endif
		}
	for ( i = 0; i < ncmap; i++ )
	    for ( j = 0; j < maplen; j++ )
		cmap[i][j] = gammap[cmap[i][j]];
	lfree((char *) gammap);		/* clean up */
    }
    return cmap;
}

/* Free up the memory used in the cmap */
void freemap(rle_pixel **cmap)
{
    int i,j;

    if(cmap != NULL)	/* be carefull */
    {
	j = (int)cmap[-1]-1;	/* recover size of cmap */
	for(i=j-1;i>=0;i--)
		if(cmap[i] != NULL && (i == 0 || cmap[i] != cmap[0]))
	    lfree((char *)cmap[i]);	/* free all its elements */
	lfree((char *) (--cmap));	/* free it */
    }
}

/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/* 
 * rle_getcom.c - Get specific comments from globals structure.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Sun Jan 25 1987
 * Copyright (c) 1987, University of Utah
 */

/*****************************************************************
 * TAG( match )
 * 
 * Match a name against a test string for "name=value" or "name".
 * If it matches name=value, return pointer to value part, if just
 * name, return pointer to NUL at end of string.  If no match, return NULL.
 *
 * Inputs:
 * 	n:	Name to match.  May also be "name=value" to make it easier
 *		to replace comments.
 *	v:	Test string.
 * Outputs:
 * 	Returns pointer as above.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
static char *
match(register char *n, register char *v)
{
    for ( ; *n != '\0' && *n != '=' && *n == *v; n++, v++ )
	;
    if (*n == '\0' || *n == '=')
    {
	if ( *v == '\0' )
	    return v;
	else if ( *v == '=' )
	    return ++v;
    }

    return NULL;
}

/*****************************************************************
 * TAG( rle_getcom )
 * 
 * Return a pointer to the value part of a name=value pair in the comments.
 * Inputs:
 * 	name:		Name part of the comment to search for.
 *	globals:	sv_globals structure.
 * Outputs:
 * 	Returns pointer to value part of comment or NULL if no match.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
char *
rle_getcom(char *name, struct sv_globals *globals)
{
    char ** cp;
    char * v;

    if ( globals->sv_comments == NULL )
	return NULL;

    for ( cp = globals->sv_comments; *cp; cp++ )
	if ( (v = match( name, *cp )) != NULL )
	    return v;

    return NULL;
}

/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 *
 *  Modified at BRL 16-May-88 by Mike Muuss to avoid Alliant STDC desire
 *  to have all "void" functions so declared.
 */
/* 
 * rle_getrow.c - Read an RLE file in.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Wed Apr 10 1985
 * Copyright (c) 1985 Spencer W. Thomas
 * 
 */

struct inst {
  unsigned opcode:8, datum:8;
};

/* read a byte from th input file */
#define BREAD1(var) (var = zgetc(infile))

/* read a little endian short from the input file */
#define BREAD2(var) (var = zgetc(infile), var |=  (zgetc(infile) << 8))

#define OPCODE(inst) (inst.opcode & ~LONG)
#define LONGP(inst) (inst.opcode & LONG)
#define DATUM(inst) (0x00ff & inst.datum)

static int	   debug_f;		/* if non-zero, print debug info */

/*****************************************************************
 * TAG( rle_get_setup )
 * 
 * Read the initialization information from an RLE file.
 * Inputs:
 * 	globals:    Contains pointer to the input file.
 * Outputs:
 * 	globals:    Initialized with information from the
 *		    input file.
 *	Returns 0 on success, -1 if the file is not an RLE file,
 *	-2 if malloc of the color map failed, -3 if an immediate EOF
 *	is hit (empty input file), and -4 if an EOF is encountered reading
 *	the setup information.
 * Assumptions:
 * 	infile points to the "magic" number in an RLE file (usually
 * byte 0 in the file).
 * Algorithm:
 * 	Read in the setup info and fill in sv_globals.
 */
int rle_get_setup(struct sv_globals *globals)
{
    struct XtndRsetup setup;
    short magic;			/* assume 16 bits */
    register ZFILE *infile = globals->svfb_fd;
    rle_pixel * bg_color;
    register int i;
    char * comment_buf;

    globals->sv_bg_color = NULL;	/* in case of error, nothing to free */
    globals->sv_cmap = NULL;
    globals->sv_comments = NULL;

    BREAD2( magic );
    if ( zeof( infile ) ) {
        debug("rlelib: EOF on reading magic\n");
	return -3;
    }
    if ( magic != XtndRMAGIC ) {
        debug("rlelib: bad magic\n");
	return -1;
    }
    BREAD2( setup.h_xpos );	/* assume VAX packing */
    BREAD2( setup.h_ypos );
    BREAD2( setup.h_xlen );
    BREAD2( setup.h_ylen );
    BREAD1( setup.h_flags );
    BREAD1( setup.h_ncolors );
    BREAD1( setup.h_pixelbits );
    BREAD1( setup.h_ncmap );
    BREAD1( setup.h_cmaplen );
    if ( zeof( infile ) ) {
        debug("rlelib: EOF on reading header\n");
	return -4;
    }

    /* Extract information from setup */
    globals->sv_ncolors = setup.h_ncolors;
    for ( i = 0; i < globals->sv_ncolors; i++ )
	SV_SET_BIT( *globals, i );

    if ( !(setup.h_flags & H_NO_BACKGROUND) )
    {
	globals->sv_bg_color = (int *)lmalloc(
	    (unsigned)(sizeof(int) * setup.h_ncolors) );
	bg_color = (rle_pixel *)lmalloc(
	    (unsigned)(1 + (setup.h_ncolors / 2) * 2) );
	zread( infile, (byte *)bg_color, 1 + (setup.h_ncolors / 2) * 2 );
	for ( i = 0; i < setup.h_ncolors; i++ )
	    globals->sv_bg_color[i] = bg_color[i];
	lfree( bg_color );
    }
    else
	zgetc( infile );			/* skip filler byte */

    if ( setup.h_flags & H_NO_BACKGROUND )
	globals->sv_background = 0;
    else if ( setup.h_flags & H_CLEARFIRST )
	globals->sv_background = 2;
    else
	globals->sv_background = 1;
    if ( setup.h_flags & H_ALPHA )
    {
	globals->sv_alpha = 1;
	SV_SET_BIT( *globals, SV_ALPHA );
    }
    else
	globals->sv_alpha = 0;

    globals->sv_xmin = setup.h_xpos;
    globals->sv_ymin = setup.h_ypos;
    globals->sv_xmax = globals->sv_xmin + setup.h_xlen - 1;
    globals->sv_ymax = globals->sv_ymin + setup.h_ylen - 1;

    globals->sv_ncmap = setup.h_ncmap;
    globals->sv_cmaplen = setup.h_cmaplen;
    if ( globals->sv_ncmap > 0 )
    {
	register int maplen =
		     globals->sv_ncmap * (1 << globals->sv_cmaplen);
	globals->sv_cmap = (rle_map *)lmalloc(
	    (unsigned)(sizeof(rle_map) * maplen) );
	if ( globals->sv_cmap == NULL )
	{
	    fprintf( stderr,
		"Malloc failed for color map of size %d*%d in rle_get_setup\n",
		globals->sv_ncmap, (1 << globals->sv_cmaplen) );
	    return -2;
	}
    	for ( i = 0; i < maplen; i++ )
    	    BREAD2( globals->sv_cmap[i] );
    }

    /* Check for comments */
    if ( setup.h_flags & H_COMMENT )
    {
	short comlen, evenlen;
	register char * cp;

	BREAD2( comlen );	/* get comment length */
	evenlen = (comlen + 1) & ~1;	/* make it even */
	comment_buf = (char *)lmalloc( (unsigned) evenlen );
	if ( comment_buf == NULL )
	{
	    fprintf( stderr,
		     "Malloc failed for comment buffer of size %d in rle_get_setup\n",
		     comlen );
	    return -2;
	}
	zread( infile, (byte *)comment_buf, evenlen );
	/* Count the comments */
	for ( i = 0, cp = comment_buf; cp < comment_buf + comlen; cp++ )
	    if ( *cp == 0 )
		i++;
	i++;			/* extra for NULL pointer at end */
	/* Get space to put pointers to comments */
	globals->sv_comments =
	    (char **)lmalloc( (unsigned)(i * sizeof(char *)) );
	if ( globals->sv_comments == NULL )
	{
	    fprintf( stderr,
		    "Malloc failed for %d comment pointers in rle_get_setup\n",
		     i );
	    return -2;
	}
	/* Get pointers to the comments */
	*globals->sv_comments = comment_buf;
	for ( i = 1, cp = comment_buf + 1; cp < comment_buf + comlen; cp++ )
	    if ( *(cp - 1) == 0 )
		globals->sv_comments[i++] = cp;
	globals->sv_comments[i] = NULL;
    }
    else
	globals->sv_comments = NULL;

    /* Initialize state for rle_getrow */
    globals->sv_private.get.scan_y = globals->sv_ymin;
    globals->sv_private.get.vert_skip = 0;
    globals->sv_private.get.is_eof = 0;
    globals->sv_private.get.is_seek = 0;	/* Can't do seek on zfile */
    debug_f = 0;

    if ( !zeof( infile ) )
	return 0;			/* success! */
    else
    {
	globals->sv_private.get.is_eof = 1;
	return -4;
    }
}


/*****************************************************************
 * TAG( rle_free_setup )
 * 
 * Free any memory allocated by rle_get_setup
 * Inputs:
 * 	globals:    Contains pointer to the input file.
 * Algorithm:
 *      Assumes non-zero values mean there is something to free.
 */
void rle_free_setup(struct sv_globals *globals)
{
    if(globals->sv_bg_color != NULL)
	lfree((char *)globals->sv_bg_color);
    if(globals->sv_cmap != NULL)
	lfree((char *)globals->sv_cmap);
    if(globals->sv_comments != NULL)
    {
	if(*globals->sv_comments != NULL)
	    lfree((char *)*globals->sv_comments);
	lfree((char *)globals->sv_comments);
    }
    globals->sv_bg_color = NULL;
    globals->sv_cmap = NULL;
    globals->sv_comments = NULL;
}

/*****************************************************************
 * TAG( rle_debug )
 * 
 * Turn RLE debugging on or off.
 * Inputs:
 * 	on_off:		if 0, stop debugging, else start.
 * Outputs:
 * 	Sets internal debug flag.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
void
rle_debug(int on_off)
{
    debug_f = on_off;
}


/*****************************************************************
 * TAG( rle_getrow )
 * 
 * Get a scanline from the input file.
 * Inputs:
 *	globals:    sv_globals structure containing information about 
 *		    the input file.
 * Outputs:
 * 	scanline:   an array of pointers to the individual color
 *		    scanlines.  Scanline is assumed to have
 *		    globals->sv_ncolors pointers to arrays of rle_pixel,
 *		    each of which is at least globals->sv_xmax+1 long.
 *	Returns the current scanline number, or -1 if there was an error.
 * Assumptions:
 * 	rle_get_setup has already been called.
 * Algorithm:
 * 	If a vertical skip is being executed, and clear-to-background is
 *	specified (globals->sv_background is true), just set the
 *	scanlines to the background color.  If clear-to-background is
 *	not set, just increment the scanline number and return.
 * 
 *	Otherwise, read input until a vertical skip is encountered,
 *	decoding the instructions into scanline data.
 */

int rle_getrow(struct sv_globals *globals, rle_pixel **scanline)
{
    register rle_pixel * scanc;
    register int nc;
    register ZFILE *infile = globals->svfb_fd;
    int scan_x = globals->sv_xmin,	/* current X position */
	   channel = 0;			/* current color channel */
    short word, long_data;
    struct inst inst;

    /* Clear to background if specified */
    if ( globals->sv_background == 2 )
    {
	if ( globals->sv_alpha && SV_BIT( *globals, -1 ) )
	    bfill( (char *)scanline[-1], globals->sv_xmax + 1, 0 );
	for ( nc = 0; nc < globals->sv_ncolors; nc++ )
	    if ( SV_BIT( *globals, nc ) )
		bfill( (char *)scanline[nc], globals->sv_xmax+1,
			globals->sv_bg_color[nc] );
    }

    /* If skipping, then just return */
    if ( globals->sv_private.get.vert_skip > 0 )
    {
	globals->sv_private.get.vert_skip--;
	globals->sv_private.get.scan_y++;
	if ( globals->sv_private.get.vert_skip > 0 )
	{
            if (debug_f)
	        fprintf(stderr, "Skipping a line\n");
	    return globals->sv_private.get.scan_y;
	}
    }

    /* If EOF has been encountered, return also */
    if ( globals->sv_private.get.is_eof )
    {
        if (debug_f)
	    fprintf(stderr, "EOF was was encountered last read.\n");
	return ++globals->sv_private.get.scan_y;
    }

    /* Otherwise, read and interpret instructions until a skipLines
     * instruction is encountered.
     */
    if ( SV_BIT( *globals, channel ) )
	scanc = scanline[channel] + scan_x;
    else
	scanc = NULL;
    for (;;)
    {
	BREAD1( inst.opcode );
	BREAD1( inst.datum );
	if ( zeof(infile) )
	{
	    if (debug_f)
		fprintf(stderr, "Got eof on read\n");
	    globals->sv_private.get.is_eof = 1;
	    break;		/* <--- one of the exits */
	}

	switch( OPCODE(inst) )
	{
	case RSkipLinesOp:
	    if ( LONGP(inst) )
	    {
	        BREAD2( long_data );
		globals->sv_private.get.vert_skip = long_data;
	    }
	    else
		globals->sv_private.get.vert_skip = DATUM(inst);
	    if (debug_f)
		fprintf(stderr, "Skip %d Lines (to %d)\n",
			globals->sv_private.get.vert_skip,
			globals->sv_private.get.scan_y +
			    globals->sv_private.get.vert_skip );

	    break;			/* need to break for() here, too */

	case RSetColorOp:
	    channel = DATUM(inst);	/* select color channel */
	    if ( channel == 255 )
		channel = -1;
	    scan_x = globals->sv_xmin;
	    if ( SV_BIT( *globals, channel ) )
		scanc = scanline[channel]+scan_x;
	    if ( debug_f )
		fprintf( stderr, "Set color to %d (reset x to %d)\n",
			 channel, scan_x );
	    break;

	case RSkipPixelsOp:
	    if ( LONGP(inst) )
	    {
	        BREAD2( long_data );
		scan_x += long_data;
		scanc += long_data;
		if ( debug_f )
		    fprintf( stderr, "Skip %d pixels (to %d)\n",
			    long_data, scan_x );
			 
	    }
	    else
	    {
		scan_x += DATUM(inst);
		scanc += DATUM(inst);
		if ( debug_f )
		    fprintf( stderr, "Skip %d pixels (to %d)\n",
			    DATUM(inst), scan_x );
	    }
	    break;

	case RByteDataOp:
	    if ( LONGP(inst) )
	    {
	        BREAD2( long_data );
		nc = (int)long_data;
	    }
	    else
		nc = DATUM(inst);
	    nc++;
	    if ( SV_BIT( *globals, channel ) )
	    {
		zread( infile, (byte *)scanc, nc );
		if ( nc & 1 )
		    (void)zgetc( infile );	/* throw away odd byte */
	    }
	    else
	    {		/* Emulate a forward fseek */
		register int ii;
		for ( ii = ((nc + 1) / 2) * 2; ii > 0; ii-- )
		    (void) zgetc( infile );	/* discard it */
	    }

	    scanc += nc;
	    scan_x += nc;
	    if ( debug_f )
	    {
		if ( SV_BIT( *globals, channel ) )
		{
		    rle_pixel * cp = scanc - nc;
		    fprintf( stderr, "Pixel data %d (to %d):", nc, scan_x );
		    for ( ; nc > 0; nc-- )
			fprintf( stderr, "%02x", *cp++ );
		    putc( '\n', stderr );
		}
	        else
		    fprintf( stderr, "Pixel data %d (to %d)\n", nc, scan_x );
	    }
	    break;

	case RRunDataOp:
	    if ( LONGP(inst) )
	    {
	        BREAD2( long_data );
		nc = long_data;
	    }
	    else
		nc = DATUM(inst);
	    scan_x += nc + 1;

	    BREAD2( word );
	    if ( debug_f )
		fprintf( stderr, "Run length %d (to %d), data %02x\n",
			    nc + 1, scan_x, word );
	    if ( SV_BIT( *globals, channel ) )
	    {
		if ( nc >= 10 )		/* break point for 785, anyway */
		{
		    bfill( (char *)scanc, nc + 1, word );
		    scanc += nc + 1;
		}
		else
		    for ( ; nc >= 0; nc--, scanc++ )
			*scanc = word;
	    }
	    break;

	case REOFOp:
	    if (debug_f)
		fprintf(stderr, "Got eof op code\n");
	    globals->sv_private.get.is_eof = 1;
	    break;

	default:
	    fprintf( stderr,
		     "rle_getrow: Unrecognized opcode: %d\n", inst.opcode );
		return (-1);
	}
	if ( OPCODE(inst) == RSkipLinesOp || OPCODE(inst) == REOFOp )
	    break;			/* <--- the other loop exit */
    }

    return globals->sv_private.get.scan_y;
}

/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 *
 *  Modified at BRL 16-May-88 by Mike Muuss to avoid Alliant STDC desire
 *  to have all "void" functions so declared.
 *
 */

/*****************************************************************
 * TAG( make_gamma )
 * 
 * Makes a gamma compenstation map.
 * Inputs:
 *  gamma:			desired gamma
 * 	gammamap:		gamma mapping array
 * Outputs:
 *  Changes gamma array entries.
 */
void make_gamma(double gamma, int *gammamap)
{
	register int i;

    for ( i = 0; i < 256; i++ )
		{
#ifdef BYTEBUG
		int byteb1;
		
		byteb1 = (int)(0.5 + 255 * pow( i / 255.0, 1.0/gamma ));
		gammamap[i] = byteb1;
#else
		gammamap[i] = (int)(0.5 + 255 * pow( i / 255.0, 1.0/gamma ));
#endif
		}
}

