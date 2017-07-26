/* zio.c:
 *
 * this properly opens and reads from an image file, compressed or otherwise.
 *
 * jim frost 10.03.89
 *
 * this was hacked on 09.12.90 to cache reads and to use stdin.
 *
 * Copyright 1989, 1990 Jim Frost.  See included file "copyright.h" for
 * complete copyright information.
 *
 * Re-worked by Graeme W. Gill on 91/12/04 to add zrewind, zunread and
 * better performance of per character reads.
 * 
 * The uu decoder is loosely based on uuexplode-1.2.c
 * by Michael Bergman (ebcrmb@ebc.ericsson.se),
 * which is based on kiss 1.0 by Kevin Yang.
 *
 */

#include "copyright.h"
#include "xli.h"
#include <ctype.h>
#include <string.h>

#ifdef VMS
#define NO_UNCOMPRESS		/* VMS doesn't have uncompress */
#endif

/* ANSI C doesn't declare popen in stdio.h ! */
FILE *popen(const char *, const char *);

#define MAX_ZFILES 32
#define UUSTARTLEN 100		/* Lines to look though before assuming not uuencoded */

static int uuread(ZFILE *zf, byte *buf, int len);
static int uugetline(ZFILE *zf, byte *obuf);
static boolean uuCheck(byte *icp, int len);
static int uuExplodeLine(byte *icp, byte *ocp);
static int buflen(byte *buf, int len);

static ZFILE ZFileTable[MAX_ZFILES];
static boolean ZForceCache = FALSE;

int zread(ZFILE *zf, byte *buf, int len)
{
	int lentoread = len;
	struct cache *dataptr = zf->dataptr;

	while (len > 0 && !zf->eof) {
		/* Read any data in cache buffers or aux buffer */
		if (zf->bufptr < zf->endptr) {
			int readlen = zf->endptr - zf->bufptr;
			if (readlen > len)
				readlen = len;
			bcopy(zf->bufptr, buf, readlen);
			buf += readlen;
			len -= readlen;
			zf->bufptr += readlen;
			continue;
		}
		/* else we have run out of buffered data */

		/* If we were reading from an aux buffer, */
		/* retore the previous state */
		if (zf->auxb != NULL) {		/* restore previous buffer */
			if (zf->auxb != zf->buf)	/* if not getc buffer */
				lfree(zf->auxb);	/* was un-read buffer */
			zf->auxb = NULL;
			zf->bufptr = zf->oldbufptr;
			zf->endptr = zf->oldendptr;
			continue;
		}
		/* If this is a newly opened file or we are */
		/* no longer reading previously cached data */
		if (dataptr == NULL) {
			if (zf->nocache) {	/* If the cache is turned off, read directly */
				if (len > 1) {	/* more than a byte being read */
					len -= uuread(zf, buf, len);
					if (len > 0)
						zf->eof = TRUE;
					continue;
				}
				/* else we are reading one byte directly - this is a little */
				/* inefficient :-). Buffer it instead. */
				zf->bufptr = zf->buf;
				zf->endptr = zf->bufptr + uuread(zf, zf->bufptr, BUFSIZ);
				if (zf->endptr == zf->bufptr)
					zf->eof = TRUE;
				continue;
			}
			/* else fall through to deal with first cached buffered read */
		} else {	/* we must move on to the next cache buffer */
			if (zf->bufptr == NULL) {	/* If this is a first re-read of the cache */
				zf->bufptr = dataptr->buf;	/* init the pointers */
				zf->endptr = dataptr->end;
				continue;
			}
			/* If there are more buffers, then advance */
			/* to the next one */
			if (dataptr->next != NULL) {
				/* advance to the next buffer */
				dataptr = zf->dataptr = dataptr->next;
				zf->bufptr = dataptr->buf;
				zf->endptr = dataptr->end;
				continue;
			}
			/* There are no more buffers */
			if (dataptr->eof) {
				zf->eof = TRUE;		/* we know that was the last one */
				continue;
			}
		}

		/* We have come to the end of the cache buffers or */
		/* we are doing the first cached read. */

		/* If the cache is off, switch it fully */
		/* out of the picture from now on */
		if (zf->nocache) {	/* read directly */
			zf->dataptr = dataptr = NULL;
			zf->bufptr = NULL;
			zf->endptr = NULL;
			continue;
		}
		/* We are caching and have to read another buffer or maybe we have */
		/* to read the very first buffer (zf->data and dataptr == NULL) */
		zf->dataptr = (struct cache *) lmalloc(sizeof(struct cache));
		zf->dataptr->next = NULL;
		zf->dataptr->eof = FALSE;
		if (zf->data == NULL)	/* If very first read */
			zf->data = zf->dataptr;		/* set to first allocated buffer */
		else		/* link buffer into chain */
			dataptr->next = zf->dataptr;
		dataptr = zf->dataptr;
		zf->bufptr = dataptr->buf;
		zf->endptr = dataptr->end = zf->bufptr + uuread(zf, zf->bufptr, BUFSIZ);
		dataptr->eof = ((zf->endptr - zf->bufptr) < BUFSIZ);
	}
	return (lentoread - len);
}


/* append the given buffer data to the files read cache */
/* (Allows reading blocks directly from input stream while */
/*  maintaining cache data) */
static void _zaptocache(ZFILE *zf, char *cbuf, int blen)
{
	struct cache *newdat, *dat;
	int len;
	while (blen > 0) {	/* until we've stashed all of input */
		len = blen;
		if (len > BUFSIZ)
			len = BUFSIZ;

		/* create new cache data block */
		newdat = (struct cache *) lmalloc(sizeof(struct cache));
		bcopy(cbuf, newdat->buf, len);
		newdat->end = newdat->buf + len;
		newdat->next = NULL;
		newdat->eof = FALSE;

		/* append it to cache buffers */
		if (zf->data == NULL) {		/* nothing read yet */
			zf->data = newdat;
			zf->dataptr = zf->data;
		} else {
			for (dat = zf->data; dat->next != NULL; dat = dat->next);	/* go to end of chain */
			dat->next = newdat;
			dat->eof = FALSE;
		}
		cbuf += len;
		blen -= len;
	}
}

/* Return the files EOF status */

int zeof(ZFILE *zf)
{
	return zf->eof;
}

void zclearerr(ZFILE *zf)
{
	clearerr((zf)->stream);
	zf->eof = 0;
}


/* reset a read cache (reset means really close the file)
 */

void zreset(char *filename)
{
	int a;
	ZFILE *zf;

	if (TRUE == ZForceCache)
		return;

	/* if NULL filename, reset the entire table
	 */
	if (!filename) {
		for (a = 0; a < MAX_ZFILES; a++)
			if (ZFileTable[a].filename)
				zreset(ZFileTable[a].filename);
		return;
	}
	for (zf = ZFileTable; zf < (ZFileTable + MAX_ZFILES); zf++)
		if (zf->filename && !strcmp(filename, zf->filename))
			break;

	if (zf == (ZFileTable + MAX_ZFILES))	/* no go joe */
		return;

	_zreset(zf);
}

/* reset by file descriptor */
void _zreset(ZFILE *zf)
{
	struct cache *old;
	if (zf->dataptr != zf->data)
		fprintf(stderr, "zreset: warning: ZFILE for %s was not closed properly\n",
			zf->filename);
	while (zf->data) {
		old = zf->data;
		zf->data = zf->data->next;
		lfree((byte *) old);
	}
	lfree((byte *) zf->filename);
	zf->filename = NULL;
	zf->data = NULL;
	zf->dataptr = NULL;
	zf->nocache = FALSE;
	if (zf->auxb != NULL && zf->auxb != zf->buf)
		lfree((byte *) zf->auxb);
	zf->auxb = NULL;
	zf->bufptr = NULL;
	zf->endptr = NULL;
	zf->eof = FALSE;

	switch (zf->type) {
	case ZSTANDARD:
		fclose(zf->stream);
		break;
#ifndef NO_UNCOMPRESS
	case ZPIPE:
		pclose(zf->stream);
		break;
#endif				/* NO_UNCOMPRESS */
	case ZSTDIN:
		break;
	default:
		fprintf(stderr, "zreset: bad ZFILE structure\n");
		exit(1);
	}
}

/* discard all data that has been read. Return to state just after file was opened */
void _zclear(ZFILE *zf)
{
	struct cache *old;

	if (zf->auxb != NULL && zf->auxb != zf->buf)
		lfree(zf->auxb);

	while (zf->data) {
		old = zf->data;
		zf->data = zf->data->next;
		free(old);
	}
	zf->auxb = NULL;
	zf->bufptr = NULL;
	zf->endptr = NULL;
	zf->eof = FALSE;
	zf->data = NULL;
	zf->dataptr = NULL;
	zf->nocache = FALSE;
}

ZFILE *zopen(char *name)
{
	ZFILE *zf;

	/* look for filename in open file table
	 */

	for (zf = ZFileTable; zf < (ZFileTable + MAX_ZFILES); zf++)
		if (zf->filename && !strcmp(name, zf->filename)) {

			/* if we try to reopen a file whose caching was
			 * disabled, warn the user and try to recover.
			 * we cannot recover if it was stdin.
			 */

			if (zf->nocache) {
				if (zf->type == ZSTDIN) {
					fprintf(stderr, "zopen: caching was disabled by previous caller; can't reopen stdin\n");
					return (NULL);
				}
				fprintf(stderr, "zopen: warning: caching was disabled by previous caller\n");
				zreset(zf->filename);	/* remove entry and treat like new open */
				break;
			}
			if (zf->dataptr != zf->data)
				fprintf(stderr, "zopen: warning: file doubly opened\n");
			zf->dataptr = zf->data;		/* re-start with cache if it exists */
			if (zf->auxb != NULL && zf->auxb != zf->buf)
				lfree((byte *) zf->auxb);
			zf->auxb = NULL;
			zf->bufptr = NULL;
			zf->endptr = NULL;
			zf->eof = FALSE;
			return (zf);
		}
	/* find unused ZFileTable entry
	 */

	for (zf = ZFileTable; zf < (ZFileTable + MAX_ZFILES) && zf->filename; zf++)
		/* EMPTY */
		;

	if (zf >= (ZFileTable + MAX_ZFILES)) {
		fprintf(stderr, "zopen: no more files available\n");
		exit(1);
	}
	zf->filename = dupString(name);
	if (!_zopen(zf)) {	/* failed */
		lfree(zf->filename);
		zf->filename = NULL;
		return (NULL);
	}
	return (zf);
}

/* Do an open given a file pointer */
/* Return TRUE if open suceeded */
boolean
_zopen(ZFILE *zf)
{
	char *name = zf->filename;
	char uuibuf[UULEN], uudest[UULEN], uudummy[UULEN];
	int uumode, uutry = UUSTARTLEN;

	zf->data = NULL;
	zf->dataptr = NULL;
	zf->auxb = NULL;
	zf->bufptr = NULL;
	zf->endptr = NULL;
	zf->nocache = FALSE;
	zf->eof = FALSE;
	zf->uudecode = FALSE;

	/* file filename is `stdin' then use stdin
	 */

	if (!strcmp(name, "stdin")) {
		zf->type = ZSTDIN;
		zf->stream = stdin;
	} else {
		char const *cmd = 0;

#ifndef NO_UNCOMPRESS
		/* if filename ends in `.Z' then open pipe to uncompress.  if
		 * your system doesn't have uncompress you can define
		 * NO_UNCOMPRESS and it just won't check for this.
		 */

#ifdef HAVE_GUNZIP
		if (
			(strlen(name) > 3 &&
			!strcasecmp(".gz", name + strlen(name) - 3)) ||
			(strlen(name) > 2 &&
			!strcasecmp(".Z", name + strlen(name) - 2))
		) {
			cmd = "gunzip -c ";
		}
#else
		/* it's a unix compressed file, so use uncompress */
		if ((strlen(name) > (unsigned) 2) &&
				!strcmp(".Z", name + (strlen(name) - 2))) {
			cmd = "uncompress -c ";
		}
#endif /* HAVE_GUNZIP */
#ifdef HAVE_BUNZIP2
		if (strlen(name) > 4 &&
				!strcasecmp(".bz2", name + strlen(name) - 4)) {
			cmd = "bunzip2 -c ";
		}
#endif /* HAVE_BUNZIP2 */
#endif /* NO_UNCOMPRESS */

		if (cmd) {
			char *buf, *s, *t;

			/* protect in single quotes, replacing single quotes
			 * with '"'"', so worst-case expansion is 5x
	  		 */
			buf = (char *) lmalloc(
				strlen(cmd) + 1 + 5 * strlen(name) + 1 + 1);
			strcpy(buf, cmd);
			s = buf + strlen(buf);
			*s++ = '\'';
			for (t = name; *t; ++t) {
				if ('\'' == *t) {
					strcpy(s, "'\"'\"'");
					s += strlen(s);
				} else {
					*s++ = *t;
				}
			}
			*s++ = '\'';
			*s = '\0';

			zf->type = ZPIPE;
			zf->stream = popen(buf, "r");
			lfree(buf);
		} else {
			/* default to normal stream
			 */
			zf->type = ZSTANDARD;
#ifdef VMS
			zf->stream = fopen(name, "r", "ctx=bin", "ctx=stm",
				"rfm=stmlf");
#else
			zf->stream = fopen(name, "r");
#endif
		}
	}

	if (!zf->stream) {
		return (FALSE);
	}

	/* File is now open, so see if it is a uuencoded file */
	while (uutry-- > 0) {
		int blen;
		if ((blen = fread(uuibuf, 1, UULEN, zf->stream)) > 0) {
			_zaptocache(zf, uuibuf, blen);	/* keep zfile data cached */
			if (!strncmp(uuibuf, "begin ", 6)
			    && isdigit(uuibuf[6])
			    && isdigit(uuibuf[7])
			    && isdigit(uuibuf[8])) {
				/* Found a begin line */
				/* Extract filename and file mode       */
				if ((sscanf(uuibuf, "begin %o%[ ]%s", &uumode, uudummy, uudest)) != 3) {
					continue;	/* it's not right */
				}
				/* Need access to verbose flag to print the following */
				/* printf("uuencoded file '%s' being decoded\n",uudest); */
				_zclear(zf);	/* discard all data read */
				zf->uudecode = TRUE;
				zf->uunext = zf->uuend = &zf->uubuf[0];
				zf->uustate = UUBODY;
				return (TRUE);
			}
		}
	}

	/* Not uu encodede */
	return (TRUE);
}

/* The function zgetc, rather than the macro */
int _zgetc(ZFILE *zf)
{
	unsigned char c;

	if (zf->bufptr < zf->endptr)
		return *zf->bufptr++;
	if (zread(zf, &c, 1) > 0)
		return (c);
	else
		return (EOF);
}

char *zgets(char *buf, unsigned int size, ZFILE *zf)
{
	if ((--size) <= 0)
		return NULL;
	{
		int ssize = zf->endptr - zf->bufptr;
		if (ssize > 0) {	/* can do this fast */
			register byte *bp = zf->bufptr, *be;
			be = zf->bufptr + (size < ssize ? size : ssize);
			while (bp < be) {
				if (*bp++ == '\n')
					break;
			}
			if (bp < be || size < ssize) {
				bcopy(zf->bufptr, buf, bp - zf->bufptr);
				buf[bp - zf->bufptr] = '\000';
				zf->bufptr = bp;
				return ((char *) buf);
			}
		}
	}
	/* do this slower */
	{
		register char *cp = buf;
		register int c;
		while (cp < (buf + size) && (c = zgetc(zf)) != EOF) {
			*cp++ = c;
			if (c == '\n')
				break;
		}
		if (cp > buf) {	/* we read something */
			*cp = '\000';
			return ((char *) buf);
		}
	}
	return (NULL);
}

/* Return a block of data back to the input stream.
 * Usefull if a load routine does its own buffering
 * and wants to return what is left after it has read an image.
 */
void zunread(ZFILE *zf, byte const *buf, int len)
{
	byte *new;
	int sizeaxb;		/* size of aux buffer */
	int noinaxb;		/* number unread in aux buffer */
	int nofraxb;		/* number free in aux buffer */

	if (len == 0)
		return;
	if (zf->auxb != NULL) {
		sizeaxb = zf->endptr - zf->auxb;	/* size of aux buffer */
		noinaxb = zf->endptr - zf->bufptr;	/* number unread in aux buffer */
		nofraxb = zf->bufptr - zf->auxb;	/* number free in aux buffer */
	} else {
		zf->oldbufptr = zf->bufptr;
		zf->oldendptr = zf->endptr;
		sizeaxb = noinaxb = nofraxb = 0;
	}
	if (len <= nofraxb) {	/* no need to alloc more */
		zf->bufptr -= len;
		bcopy(buf, zf->bufptr, len);
	} else {		/* need some more space */
		int extra = 0;
		if (len < 100)
			extra = 100;
		new = (byte *) lmalloc(extra + noinaxb + len);
		bcopy(buf, new + extra, len);	/* copy new aux data */
		if (noinaxb != 0) {	/* copy old data */
			bcopy(zf->bufptr, new + extra + len, noinaxb);
			if (zf->auxb != NULL && zf->auxb != zf->buf)
				lfree(zf->auxb);
		}
		zf->auxb = new;
		zf->bufptr = new + extra;
		zf->endptr = new + extra + noinaxb + len;
	}
	zf->eof = FALSE;
}


/* this turns off caching when an image has been identified and we will not
 * need to re-open it
 */

void znocache(ZFILE *zf)
{
	if (FALSE == ZForceCache)
		zf->nocache = TRUE;
}

void zforcecache(boolean v)
{
	ZForceCache = v;
}

/* reset cache pointers in a ZFILE.  nothing is actually reset until a
 * zreset() is called with the filename.
 */

void zclose(ZFILE *zf)
{
	zf->dataptr = zf->data;
	if (zf->auxb != NULL && zf->auxb != zf->buf)
		lfree(zf->auxb);
	zf->auxb = NULL;
	zf->bufptr = NULL;
	zf->endptr = NULL;
	zf->eof = FALSE;
}

/* close the file and then re-open it. */
/* Return TRUE on sucess. */
boolean
_zreopen(ZFILE *zf)
{
	char *tname;
	tname = dupString(zf->filename);
	_zreset(zf);
	zf->filename = tname;
	if (!_zopen(zf)) {	/* failed */
		lfree(zf->filename);
		zf->filename = NULL;
		return (FALSE);
	}
	return (TRUE);
}

/* Rewind the cached file. Warn the user if 
 * this is not likely to work.
 * Return TRUE on sucess.
 */

boolean
zrewind(ZFILE *zf)
{
	if (zf->nocache) {
		if (zf->type == ZSTDIN) {
			fprintf(stderr, "zrewind: caching was disabled by previous caller; can't rewind\n");
			return (FALSE);
		}
		fprintf(stderr, "zrewind: warning: caching was disabled by previous caller\n");
		return !_zreopen(zf);
	}
	zf->dataptr = zf->data;
	if (zf->auxb != NULL && zf->auxb != zf->buf)
		lfree(zf->auxb);
	zf->auxb = NULL;
	zf->bufptr = NULL;
	zf->endptr = NULL;
	zf->eof = FALSE;
	return TRUE;
}

/* Read in a buffer from the possibly uuencoded input stream */
int uuread(ZFILE *zf, byte *buf, int len)
{
	int cl, ld = 0;
	if (!zf->uudecode) {
		if (zf->type == ZSTANDARD)
			return fread(buf, 1, len, zf->stream);

		/* Work around for SVR4 fread() bug */
		for (;;) {
			ld += cl = fread(buf, 1, len - ld, zf->stream);
			if (ld == len)
				break;
			buf += cl;
			if (feof(zf->stream))
				break;
		}
		return ld;
	}
	for (;;) {
		if (zf->uunext >= zf->uuend) {	/* if buffer empty */
			zf->uunext = zf->uubuf;
			zf->uuend = zf->uubuf + uugetline(zf, zf->uubuf);
			if (zf->uunext >= zf->uuend) {	/* nothing read */
				return ld;
			}
		}
		/* Copy what we got into the buffer */
		cl = zf->uuend - zf->uunext;	/* copy length */
		if (cl > (len - ld))
			cl = (len - ld);
		bcopy(zf->uunext, buf, cl);
		zf->uunext += cl;
		ld += cl;	/* bump length done */
		buf += cl;
		if (ld >= len) {	/* Finished */
			return ld;
		}
	}
}

/* Get a line of uudecoded bytes from the input stream */

#define SHORTALOWANCE 6		/* alowance for truncation of ' 's at the end of lines */

int uugetline(ZFILE *zf, byte *obuf)
{
	int ulen, len;		/* number of uu encoding characters */
	byte ibuf[UULEN];	/* raw line buffer */
	int nib = 0;		/* number in buffer */

	for (;;) {		/* until we return something */
		if (zf->uustate == UUEOF) {	/* there's nothing more to read */
			return 0;
		}
		if (fgets((char *) ibuf, UULEN, zf->stream) == NULL) {
			fprintf(stderr, "uudecode: unexpected EOF\n");
			zf->uustate = UUEOF;
			return 0;
		}
		len = strlen((char *) ibuf) - 2;
		ulen = ((int) (((*ibuf) - ' ') & 077) * 4 + 2) / 3;
		if (*ibuf == 'M' && (len + SHORTALOWANCE) >= ulen && uuCheck(ibuf + 1, ulen <= len ? ulen : len)) {
			if (len < ulen) {	/* we have a slightly short line */
				fprintf(stderr, "uudecode: warning, short line\n");
				while (len < ulen)
					ibuf[++len] = ' ';
			}
			nib = uuExplodeLine(ibuf, obuf);
			zf->uustate = UUBODY;
			return nib;
		}
		/* Found a possibly dodgey line */
		if (zf->uustate == UUSKIP) {
			continue;	/* Skip any non 'M' lines */
		}
		if ((*ibuf != ' ' && *ibuf != '`') || (len != 0 && len != 1)) {
			/* not a blank before end line */

			/* Is zf->uustate == UUBODY and possible dodgy line */
			if (*ibuf > 'L' || *ibuf <= ' ' || len < ulen || !uuCheck(ibuf + 1, ulen)) {
				/* not a possible short line */
				zf->uustate = UUSKIP;
				continue;
			}
			nib = uuExplodeLine(ibuf, obuf);	/* decode possible short line */

			/* if this is a valid short line, then we expect a */
			/* specific line followed by an end line */
			if (fgets((char *) ibuf, UULEN, zf->stream) == NULL) {
				fprintf(stderr, "uudecode: unexpected EOF\n");
				zf->uustate = UUEOF;
				return nib;
			}
			len = strlen((char *) ibuf) - 2;
			ulen = ((int) (((*ibuf) - ' ') & 077) * 4 + 2) / 3;
			if (*ibuf == 'M' && (len + SHORTALOWANCE) >= ulen && uuCheck(ibuf + 1, ulen <= len ? ulen : len)) {
				/* Found an 'M' line - previous line must be garbage */
				if (len < ulen) {	/* we have a slightly short line */
					fprintf(stderr, "uudecode: warning, short line\n");
					while (len < ulen)
						ibuf[++len] = ' ';
				}
				nib = uuExplodeLine(ibuf, obuf);
				zf->uustate = UUBODY;
				return nib;
			}
			if ((*ibuf != ' ' && *ibuf != '`') || (len != 0 && len != 1)) {
				/* Not found correct thing after short line. Assume garbage */
				nib = 0;
				zf->uustate = UUSKIP;
				continue;
			}
		}
		/* Check for an end line */
		if (fgets((char *) ibuf, UULEN, zf->stream) == NULL) {
			fprintf(stderr, "uudecode: unexpected EOF\n");
			zf->uustate = UUEOF;
			return nib;
		}
		len = strlen((char *) ibuf) - 2;
		ulen = ((int) (((*ibuf) - ' ') & 077) * 4 + 2) / 3;
		if (*ibuf == 'M' && (len + SHORTALOWANCE) >= ulen && uuCheck(ibuf + 1, ulen <= len ? ulen : len)) {
			/* Found an 'M' line - previous lines must be garbage */
			if (len < ulen) {	/* we have a slightly short line */
				fprintf(stderr, "uudecode: warning, short line\n");
				while (len < ulen)
					ibuf[++len] = ' ';
			}
			nib = uuExplodeLine(ibuf, obuf);
			zf->uustate = UUBODY;
			return nib;
		}
		if (len != 2 || strncmp((char *) ibuf, "end", 3)) {
			/* Not found correct thing after short line. Assume garbage */
			nib = 0;
			zf->uustate = UUSKIP;
			continue;
		}
		/* Found end line, assume short line was ok */
		zf->uustate = UUEOF;
		return nib;
	}
}

#define DEC(C)	((unsigned)(((C) - ' ') & 077))

int uuExplodeLine(byte *icp, byte *ocp)
{
	unsigned int x, y, z;
	int i, len;
	i = len = DEC(*icp);
	++icp;

	while (i > 0) {
		if (i >= 3) {
			x = (DEC(*icp) << 2);
			icp++;
			x |= (DEC(*icp) >> 4);
			y = (DEC(*icp) << 4);
			icp++;
			y |= (DEC(*icp) >> 2);
			z = (DEC(*icp) << 6);
			icp++;
			z |= (DEC(*icp));
			*ocp++ = x;
			*ocp++ = y;
			*ocp++ = z;
		} else if (i >= 2) {
			x = (DEC(*icp) << 2);
			icp++;
			x |= (DEC(*icp) >> 4);
			y = (DEC(*icp) << 4);
			icp++;
			y |= (DEC(*icp) >> 2);
			*ocp++ = x;
			*ocp++ = y;
		} else if (i >= 1) {
			x = (DEC(*icp) << 2);
			icp++;
			x |= (DEC(*icp) >> 4);
			*ocp++ = x;
		}
		icp++;
		i -= 3;
	}
	return len;
}

/* Check for valid uu encoding characters */
boolean
uuCheck(byte *icp, int len)
{
	for (; len > 0; len--, icp++)
		if (*icp >= 'a' || *icp < ' ')
			return FALSE;
	return TRUE;
}


/* Find the length of a gets buffer */
/* Return the negative length if it looks like binary */
/* junk */
static int buflen(byte *buf, int len)
{
	int i, j = 0;
	len--;			/* allow for '\000' at end */
	for (i = 0; i < len && *buf != '\n'; i++, buf++) {
		if (*buf == '\000')
			j = 1;
	}
	if (i < len)
		i++;		/* count '\n' */
	if (j)
		return -i;
	return i;
}
