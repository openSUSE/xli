/* #ident "@(#)x11:contrib/clients/xloadimage/pcd.h 1.2 94/07/29 Labtam" */
/*  */
/*
 * pcd.h - header file for Photograph on CD image files.
 */
 
#define PCDSHIFT 11
#define PCDBLOCK (1 << PCDSHIFT)
#define PCDBMASK (PCDBLOCK-1)

#define BASEW 768
#define BASEH 512

#define SZ_B16	-2
#define SZ_B4	-1
#define SZ_B	0
#define SZ_4B	1
#define SZ_16B	2

#define PCD_NO_ROTATE 0
#define PCD_ACLOCK_ROTATE 1
#define PCD_CLOCK_ROTATE 2

/* PCD image information */
typedef struct {
	ZFILE *zf;
	unsigned long off;		/* File byte offset of the start of the buffer */
	int nob;				/* Number of bytes currently in buffer */
	byte buf[PCDBLOCK];		/* current block data */

	byte *bp;				/* Current byte pointer */
	unsigned int bits;		/* bits buffer */
	int	bytes_left;			/* Bytes left from *bp */
	int	bits_left;			/* Bits left in bits */

	char *name;			/* stash pointer to name here too */
	char magic[8];		/* image format ident string (?) */
	char source[89];	/* source (?) */
	char owner[21];		/* owner (?) */
	int rotate;			/* rotation needed */

	int size;			/* Size code */
	int width,height;	/* Target width/height */
	unsigned int xzoom,yzoom;	/* User specified zoom percentages */

	byte *Lp,*C1p,*C2p;	/* Pointers to the color components */
	} pcdHeader;


/* Huffman entry (16 bit signed int) */
typedef struct {
	byte l;			/* number of bits */
	sbyte v;		/* value */
	} huff;

/* The file offset of the next read */
#define pcd_offset(hp) ((hp)->off + (hp)->nob)



