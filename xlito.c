#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include "ddxli.h"
#include "xlito.h"
#ifndef VMS
#include <unistd.h>
#endif
#include <string.h>

#define VERSION "1"
#define PATCHLEVEL "02"

#define PADSIZE 20		/* allow a little over-read on image load */

#if (defined(SYSV) || defined (VMS)) && !defined(SVR4) && !defined(__hpux) && !defined(_CRAY)
#define NEED_FTRUNCATE		/* define this if your system doesn't have ftruncate() */
#endif

#define SHOW 0
#define XLI 1
#define CHANGE 2
#define DELETE 3

static char *find_text(int file, char *key, char **junk, int *jlen, int show,
	int delete);
static void usage(char *pname);
char *pname, *fname;

#undef  DEBUG

#ifdef DEBUG
# define debug(xx)	fprintf(stderr,xx)
#else
# define debug(xx)
#endif


int main(int argc, char **argv)
{
	int func = SHOW;
	char *text = 0;
	int a = 1;
	char *key = KEY;
	int klen;
	char *junk;
	int jlen;
	int file;
	int nameopt = 0;

	pname = argv[0];
	klen = strlen(key);
	if (argc < 2)
		usage(pname);

	if (argv[a][0] == '-')
	{
		if (argv[a][1] == 'c')
		{
			if (argv[a][2] != '\000')
				text = &argv[a][2];
			else if (argc >= 3)
				text = argv[++a];
			else
				usage(pname);
			func = CHANGE;
		}
		else if (argv[a][1] == 'd')
		{
			if (argv[a][2] != '\000')
				usage(pname);
			func = DELETE;
		}
		else if (argv[a][1] == 'x')
		{
			if (argv[a][2] != '\000')
				usage(pname);
			func = XLI;
		}
		else
			usage(pname);
		a++;
	}

    for(;a < argc; a++)	/* Do all the files */
	{
		fname = argv[a];
		if (func == XLI)
		{
			if (nameopt)
			{
				nameopt = 0;
				file = open(fname,O_RDONLY);
				if (file != -1)
				{
					if ((text = find_text(file,key,&junk,&jlen, 1, 0)) != NULL)
						printf(" %s -name %s",text,fname);
					else
						printf(" -name %s",fname);
					close(file);
					continue;
				}
				printf(" -name %s",fname);
				continue;
			}
			if (argv[a][0] != '-')
			{
				file = open(fname,O_RDONLY);
				if (file != -1)
				{
					if ((text = find_text(file,key,&junk,&jlen, 1, 0)) != NULL)
						printf(" %s %s",text,fname);
					else
						printf(" %s",fname);
					close(file);
					continue;
				}
			}
			else
			{
				if(strcmp("-name",fname)==0)
				{
					nameopt = 1;
					continue;
				}
			}
			printf(" %s",fname);
			continue;
		}
		else if (func == SHOW)
		{
			file = open(fname,O_RDONLY);
			if (file != -1)
			{
				if ((text = find_text(file,key,&junk,&jlen, 1, 0)) != NULL)
				{
					printf("%s: '%s'\n",fname, text);
				}
				else
				{
					printf("%s: none\n",fname);
				}
				if(close(file) != 0)
				{
					fprintf(stderr,"%s: Error closing %s, ",pname,fname); perror("");
					exit(-1);
				}
				continue;
			}
			fprintf(stderr,"Unable to open file %s",fname);
			continue;
		}
		else if (func == DELETE)
		{
			file = open(fname,O_RDWR);
			if (file == -1)
			{
				fprintf(stderr,"%s: Unable to open file '%s' for deleting options, ",pname,fname); perror("");
				exit(-1);
			}
			if ((text = find_text(file,key,&junk,&jlen, 0, 1)) != NULL)
			{
				long fsize;
				fsize = lseek(file, 0L,1);
				if(ftruncate(file, fsize) != 0)
				{
					fprintf(stderr,"%s: Error truncating %s, ",pname,fname); perror("");
					exit(-1);
				}
			}
			if(close(file) != 0)
			{
				fprintf(stderr,"%s: Error closing %s, ",pname,fname); perror("");
				exit(-1);
			}
			continue;
		}
		else if (func == CHANGE)
		{
			char padding[PADSIZE];
			int i;
			char tt[6];
			int tlen;
			long fsize;
			
			tlen = strlen(text);
			if (tlen > (TOBUFSZ - klen - 4))
			{
				fprintf(stderr,"%s: text is too long\n",pname);
				exit(-1);
			}
				
			file = open(fname,O_RDWR);
			if (file == -1)
			{
				fprintf(stderr,"%s: Unable to open file '%s' for changing options, ",pname,fname); perror("");
				exit(-1);
			}
			if (!find_text(file,key,&junk,&jlen,0, 0))
			{	/* No padding there already, so add some */
				for(i=0; i < PADSIZE; i++) padding[i] = '0' + (i % 10);
				if (write(file, padding,PADSIZE) != PADSIZE)
				{
					fprintf(stderr,"%s: Error writing to file %s, ",pname,fname); perror("");
					exit(-1);
				}
			}
	
			sprintf(tt+1,"%04d",tlen);
			tt[0] = tt[ 5] = '"';
	
			if (write(file, key,klen) != klen
			|| write(file, tt + 1,5) != 5
			|| write(file, text,tlen) != tlen
			|| write(file, tt,5) != 5
			|| write(file, key,klen) != klen
			|| write(file, junk,jlen) != jlen)
			{
				fprintf(stderr,"%s: Error writing to file %s, ",pname,fname); perror("");
				exit(-1);
			}
			fsize = lseek(file, 0L,1);
			if(ftruncate(file, fsize) != 0)
			{
				fprintf(stderr,"%s: Error truncating %s, ",pname,fname); perror("");
				exit(-1);
			}
			if(close(file) != 0)
			{
				fprintf(stderr,"%s: Error closing %s, ",pname,fname); perror("");
				exit(-1);
			}
		}
	}
	return 0;
}


char buf1[TOBUFSZ];
char buf[TOBUFSZ];

/*
show == Non zero if fail gracefully
delete == Non zero to position at padding rather than at options
*/
static char *find_text(int file, char *key, char **junk, int *jlen, int show,
	int delete)
{
	int klen;
	int count;
	int tlen;
	long foff, fsize, sk;
	char *pp, sc;
	
	klen = strlen(key);

	fsize = foff = lseek(file, 0L,2);

	/* Look for key backwards in file up to TOBUFSZ chars from end */
	sk = TOBUFSZ < fsize ? TOBUFSZ : fsize;
	if (lseek(file, -sk, 2) == -1L)
		goto seekerr;
	if ((count = read(file, buf1, sk)) != sk)
	{
		debug("didn't read key\n");
		goto fail;
	}
	sc = key[klen-1];
		
	for(pp = &buf1[sk-1];;pp--)
	{
		for(;*pp != sc && pp >= buf1; pp--);
		if(pp < (buf1 + klen -1))
		{
			debug("couldn't find key\n");
			goto fail;
		}
		if (strncmp(key,pp - klen + 1,klen) ==0)
			break;
	}
	/* found the last key string, so read the text length */
	*junk = pp + 1;
	*jlen = count + (buf1 - pp) - 1;
	sk = count + (buf1 - pp) - 1 + klen + 5;
	if ((foff = lseek(file, -(sk < fsize ? sk : fsize),2)) == -1L)
		goto seekerr;
	count = read(file, buf,5);
	if (count != 5)
	{
		debug("didn't read key\n");
		goto fail;
	}
	foff += count;
	if (buf[0] != '"' || !isdigit(buf[1]) || !isdigit(buf[2]) || !isdigit(buf[3]) || !isdigit(buf[4]))
	{
		debug("length wasn't recognised\n");
		goto fail;
	}
	buf[5] = '\000';
	tlen = atoi(&buf[1]);
	if (tlen > (TOBUFSZ - klen - 5))
	{
		debug("text was too long\n");
		goto fail;
	}
	sk = tlen + klen + 10;
	if ((foff = lseek(file, -(sk < foff ? sk : foff),1)) == -1L)
		goto seekerr;
	count = read(file, buf,(tlen + klen + 5));
	if (count != (tlen + klen + 5))
	{
		debug("didn't read key + text\n");
		goto fail;
	}
	foff += count;
	if (strncmp(key,buf,klen) !=0)
	{
		debug("first key wasn't recognised\n");
		goto fail;
	}
	if (!isdigit(buf[klen]) || !isdigit(buf[klen+1]) || !isdigit(buf[klen+2]) || !isdigit(buf[klen+3]) || buf[klen+4] != '"')
	{
		debug("first length wasn't recognised\n");
		goto fail;
	}
	buf[klen+4] = '\000';
	if (tlen != atoi(&buf[klen]))
	{
		debug("first length didn't match\n");
		goto fail;
	}
	sk = tlen + klen + 5;
	if (delete)
		sk += PADSIZE;
	if ((foff = lseek(file, -(sk < foff ? sk : foff),1)) == -1L)
		goto seekerr;
	buf[klen + 5 + tlen] = '\000';
	return &buf[klen + 5];

fail:
	*jlen = 0;
	if(lseek(file,0L,2) != -1L)
		return NULL;
seekerr:
	*jlen = 0;
	if (show)
		return NULL;
	fprintf(stderr,"%s: Error seeking on file %s, ",pname,fname); perror("");
	exit(-1);
}

static void usage(char *name)
{
	fprintf(stderr,"%s: Version %s.%s usage\n",name,VERSION,PATCHLEVEL);
	fprintf(stderr,"%s [-x]|[-c text]|[-d] files\n",name);
	fprintf(stderr,"    -x          xli command line processing\n");
	fprintf(stderr,"    -c text     replace options with \"text\"\n");
	fprintf(stderr,"    -d          delete options\n");
	exit(-1);
}

#ifdef NEED_FTRUNCATE
#include <sys/types.h>
#include <unistd.h>

ftruncate(fd, size)
int fd;
off_t size;
{
	struct flock ff;
	
	ff.l_whence = SEEK_SET;
	ff.l_start = size;
	ff.l_len = 0;		/* till end of file */
	return(fcntl(fd, F_FREESP, (char *)&ff));
}

#endif
