/****************************************************************
 * mac.h:
 *
 * adapted from code by Patrick Naughton (naughton@sun.soe.clarkson.edu)
 *
 * macin.h
 * Mark Majhor
 * August 1990
 *
 * routines for reading MAC files
 *
 * Copyright 1990 Mark Majhor (see the included file
 * "mrmcpyrght.h" for complete copyright information)
 *
 ****************************************************************/

# define MAC_MAGIC	0x0

typedef unsigned char BYTE;	/* 8 bits unsigned		*/

/*
 * macin return codes
 */
#define MACIN_SUCCESS       0   /* success */

#define MACIN_ERR_BAD_SD   -1   /* bad screen descriptor */
#define MACIN_ERR_BAD_SIG  -2   /* bad signature */
#define MACIN_ERR_EOD      -3   /* unexpected end of raster data */
#define MACIN_ERR_EOF      -4   /* unexpected end of input stream */
#define MACIN_ERR_FAO      -5   /* file already open */
#define MACIN_ERR_IAO      -6   /* image already open */
#define MACIN_ERR_NFO      -7   /* no file open */
#define MACIN_ERR_NIO      -8   /* no image open */

#define MACIN_ERR_BAD_SD_STR   "Bad screen descriptor"
#define MACIN_ERR_BAD_SIG_STR  "Bad signature"
#define MACIN_ERR_EOD_STR      "Unexpected end of raster data"
#define MACIN_ERR_EOF_STR      "Unexpected end of input stream"
#define MACIN_ERR_FAO_STR      "File already open"
#define MACIN_ERR_IAO_STR      "Image already open"
#define MACIN_ERR_NFO_STR      "No file open"
#define MACIN_ERR_NIO_STR      "No image open"

typedef struct {
    int err_no;
    char *name;
    } mac_err_string;

#ifdef MAC_C
mac_err_string mac_err_strings[] = {
	{MACIN_ERR_BAD_SD, MACIN_ERR_BAD_SD_STR},
	{MACIN_ERR_BAD_SIG, MACIN_ERR_BAD_SIG_STR},
	{MACIN_ERR_EOD, MACIN_ERR_EOD_STR},
	{MACIN_ERR_EOF, MACIN_ERR_EOF_STR},
	{MACIN_ERR_FAO, MACIN_ERR_FAO_STR},
	{MACIN_ERR_IAO, MACIN_ERR_IAO_STR},
	{MACIN_ERR_NFO, MACIN_ERR_NFO_STR},
	{MACIN_ERR_NIO, MACIN_ERR_NIO_STR},
	{0}
    };
#endif

#define	MAC_HDR_LEN	512
#define ADD_HDR_LEN	128
#define	MAX_LINES	720
#define	BYTES_LINE	72
