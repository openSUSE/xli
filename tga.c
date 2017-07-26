/* #ident   "@(#)x11:contrib/clients/xloadimage/tga.c 1.2 93/07/28 Labtam" */
/*
 * Read a TrueVision Targa file.
 *
 * Created for xli by Graeme Gill,
 *
 * partially based on tgatoppm Copyright by Jef Poskanzer,
 *
 * which was partially based on tga2rast, version 1.0, by Ian MacPhedran.
 *
 */

#include "xli.h"
#include "imagetypes.h"
#include "tga.h"

/* Read the header of the file, and */
/* Return TRUE if this looks like a tga file */
/* Note that since Targa files don't have a magic number, */
/* we have to be pickey about this. */
static boolean
read_tgaHeader(ZFILE *zf, tgaHeader *hp, char *name)
{
	unsigned char buf[18];

	if(zread(zf, buf, TGA_HEADER_LEN) != TGA_HEADER_LEN)
		return FALSE;

	hp->IDLength = (unsigned int)buf[0];
	hp->CoMapType = (unsigned int)buf[1];
	hp->ImgType = (unsigned int)buf[2];
	hp->Index = (unsigned int)buf[3] + 256 * (unsigned int)buf[4];
	hp->Length = (unsigned int)buf[5] + 256 * (unsigned int)buf[6];
	hp->CoSize = (unsigned int)buf[7];
	hp->X_org = (int)buf[8] + 256 * (int)buf[9];
	hp->Y_org = (int)buf[10] + 256 * (int)buf[11];
	hp->Width = (unsigned int)buf[12] + 256 * (unsigned int)buf[13];
	hp->Height = (unsigned int)buf[14] + 256 * (unsigned int)buf[15];
	hp->PixelSize = (unsigned int)buf[16];
	hp->AttBits = (unsigned int)buf[17] & 0xf;
	hp->Rsrvd = ( (unsigned int)buf[17] & 0x10 ) >> 4;
	hp->OrgBit = ( (unsigned int)buf[17] & 0x20 ) >> 5;
	hp->IntrLve = ( (unsigned int)buf[17] & 0xc0 ) >> 6;

	/* See if it is consistent with a tga files */

	if(   hp->CoMapType != 0
	   && hp->CoMapType != 1)
		return FALSE;

	if(   hp->ImgType != 1
	   && hp->ImgType != 2
	   && hp->ImgType != 3
	   && hp->ImgType != 9
	   && hp->ImgType != 10
	   && hp->ImgType != 11)
		return FALSE;

	if(   hp->CoSize != 0
	   && hp->CoSize != 15
	   && hp->CoSize != 16
	   && hp->CoSize != 24
	   && hp->CoSize != 32)
		return FALSE;

	if(   hp->PixelSize != 8
	   && hp->PixelSize != 16
	   && hp->PixelSize != 24
	   && hp->PixelSize != 32)
		return FALSE;

	if(   hp->IntrLve != 0
	   && hp->IntrLve != 1
	   && hp->IntrLve != 2)
		return FALSE;

	/* Do a few more consistency checks */
	if(hp->ImgType == TGA_Map ||
		hp->ImgType == TGA_RLEMap)
		{
		if(hp->CoMapType != 1 || hp->CoSize == 0)
			return FALSE;	/* map types must have map */
		}
	else
		{
		if(hp->CoMapType != 0 || hp->CoSize != 0)
			return FALSE;	/* non-map types must not have map */
		}

	/* can only handle 8 or 16 bit pseudo color images */
	if(hp->CoMapType != 0 && hp->PixelSize > 16)
		return FALSE;

	/* other numbers mustn't be silly */
	if(   (hp->Index + hp->Length) > 65535	
	   || (hp->X_org + hp->Width) > 65535
	   || (hp->Y_org + hp->Height) > 65535)
		return FALSE;

	/* setup run-length encoding flag */
	if(   hp->ImgType == TGA_RLEMap 
	   || hp->ImgType == TGA_RLERGB
	   || hp->ImgType == TGA_RLEMono)
		hp->RLE = TRUE;
	else
		hp->RLE = FALSE;

	hp->name = name;
	return TRUE;
	}

/* Print a brief description of the image */
static void
tell_about_tga(tgaHeader *hp)
{
	char colors[40];

	if (hp->ImgType == TGA_Map || hp->ImgType == TGA_RLEMap)
		sprintf(colors," with %d colors",hp->Length);
	else
		colors[0] = '\000';

	printf("%s is a %dx%d %sTarga %simage%s\n",
	    hp->name,
	    hp->Width, hp->Height,
		TGA_IL_TYPE(hp->IntrLve),
		TGA_IMAGE_TYPE(hp->ImgType),
		colors);
	}

int tgaIdent(char *fullname, char *name)
{
	ZFILE *zf;
	tgaHeader hdr;

	if(!(zf = zopen(fullname)))
		{
		perror("tgaIdent");
		return(0);
		}
	if (!read_tgaHeader(zf,&hdr,name))
		{
		zclose(zf);
		return 0;		/* Nope, not a Targa file */
		}
	tell_about_tga(&hdr);
	zclose(zf);
	return 1;
	}

Image *tgaLoad(char *fullname, ImageOptions *image_ops, boolean verbose)
{
	ZFILE  *zf;
	tgaHeader hdr;
	Image  *image;
	int span,hretrace,vretrace,nopix,x,y;
	byte *data,*eoi;
	unsigned int rd_count,wr_count;

	if(!(zf = zopen(fullname)))
		{
		perror("tgaIdent");
		return(0);
		}
	if(!read_tgaHeader(zf,&hdr,image_ops->name))
		{
		zclose(zf);
		return NULL;		/* Nope, not a Targa file */
		}
	if(verbose)
		tell_about_tga(&hdr);
	znocache(zf);

	/* Skip the identifier string */
	if(hdr.IDLength)
		{
		byte buf[256];
		if(zread(zf, buf, hdr.IDLength) != hdr.IDLength)
			{
			fprintf(stderr, "tgaLoad: %s - Short read within Identifier String\n",hdr.name);
			zclose(zf);
			return NULL;
			}
		}

	/* Create the appropriate image and colormap */
	if(hdr.CoMapType != 0)	/* must be 8 or 16 bit mapped type */
		{
		int i,n=0;
		byte buf[4];
		int used = (1 << hdr.PixelSize);	/* maximum numnber of colors */

		image= newRGBImage(hdr.Width, hdr.Height, hdr.PixelSize);
  		image->title= dupString(hdr.name);
		for(i = 0; i < hdr.Index; i++)	/* init bottom of colormap */
			{
			image->rgb.red[i]=
			image->rgb.green[i]=
			image->rgb.blue[i]= 0;
			}
		switch (hdr.CoSize)
			{
			case 8:		/* Gray scale color map */
				for(; i < ( hdr.Index + hdr.Length ); i++)
					{
					if(zread(zf, buf, 1) != 1)
						{
						fprintf(stderr, "tgaLoad: %s - Short read within Colormap\n",hdr.name);
						freeImage(image);
						zclose(zf);
						return NULL;
						}
					image->rgb.red[i]=
					image->rgb.green[i]=
					image->rgb.blue[i]= buf[0] << 8;
					}
			break;
	
			case 16:	/* 5 bits of RGB */
			case 15:
				for(i = hdr.Index; i < ( hdr.Index + hdr.Length ); i++)
					{
					if(zread(zf, buf, 2) != 2)
						{
						fprintf(stderr, "tgaLoad: %s - Short read within Colormap\n",hdr.name);
						freeImage(image);
						zclose(zf);
						return NULL;
						}
					image->rgb.red[i]= (buf[1] & 0x7c) << 6;
					image->rgb.green[i]= ((buf[1] & 0x03)<< 11) + ((buf[0] & 0xe0)<< 3);
					image->rgb.blue[i]= (buf[0] & 0x1f)<< 8;
					}
				break;
	
			case 32:	/* 8 bits of RGB */
				n = 1;
			case 24:
				n += 3;
				for(i = hdr.Index; i < ( hdr.Index + hdr.Length ); i++)
					{
					if(zread(zf, buf, n) != n)
						{
						fprintf(stderr, "tgaLoad: %s - Short read within Colormap\n",hdr.name);
						freeImage(image);
						zclose(zf);
						return NULL;
						}
					image->rgb.red[i]= buf[2] << 8;
					image->rgb.green[i]= buf[1] << 8;
					image->rgb.blue[i]= buf[0] << 8;
					}
				break;
			}
		for(; i < used; i++)	/* init rest of colormap */
			{
			image->rgb.red[i]=
			image->rgb.green[i]=
			image->rgb.blue[i]= 0;
			}
		/* Don't know how many colors are actually used. */
		/* (believing the header caould cause a fault) */
		/* compress() will figure it out. */
  		image->rgb.used= used;		/* so use max possible */
		}
	else if(hdr.PixelSize == 8)		/* Have to handle gray as pseudocolor */
		{
		int i;

		image= newRGBImage(hdr.Width, hdr.Height, 8);
  		image->title= dupString(hdr.name);
		for(i = 0; i < 256; i++)
			{
			image->rgb.red[i]=
			image->rgb.green[i]=
			image->rgb.blue[i]= i << 8;
			}
  		image->rgb.used= 256;
		}
	else	/* else must be a true color image */
		{
		image= newTrueImage(hdr.Width, hdr.Height);
  		image->title= dupString(hdr.name);
		}

	/* work out virtical advance and retrace */
	if(hdr.OrgBit)	/* if upper left */
		span = hdr.Width;
	else
		span = -hdr.Width;

	if (hdr.IntrLve == TGA_IL_Four )
		hretrace = 4 * span - hdr.Width;	
	else if(hdr.IntrLve == TGA_IL_Two )
		hretrace = 2 * span - hdr.Width;	
	else
		hretrace = span - hdr.Width;	
	vretrace = (hdr.Height-1) * -span;

	nopix = (hdr.Width * hdr.Height);	/* total number of pixels */

	hretrace *= image->pixlen;	/* scale for byes per pixel */
	vretrace *= image->pixlen;
	span *= image->pixlen;

	if(hdr.OrgBit)	/* if upper left */
		{
		data = image->data;
		eoi = image->data + (nopix * image->pixlen);
		}
	else
		{
		eoi = image->data + span;
		data = image->data + (nopix * image->pixlen) + span;
		}

	/* Now read in the image data */
	rd_count = wr_count = 0;
	if (!hdr.RLE)	/* If not rle, all are literal */
		wr_count = rd_count = nopix;
	for(y = hdr.Height; y > 0; y--, data += hretrace)
		{
		byte buf[6];	/* 0, 1 for pseudo, 2, 3, 4 for BGR */

		if (data == eoi)		/* adjust for possible retrace */
			{
			data += vretrace;
			eoi += span;
			}

		/* Do another line of pixels */
		for(x = hdr.Width; x > 0; x--)
			{
			if(wr_count == 0)
				{ /* Deal with run length encoded. */
				if(zread(zf, buf, 1) != 1)
					goto data_short;
				if(buf[0] & 0x80)
					{	/* repeat count */
					wr_count = (unsigned int)buf[0] - 127;	/* no. */
					rd_count = 1;	/* need to read pixel to repeat */
					}
				else	/* number of literal pixels */
					{
					wr_count = 
					rd_count = buf[0] + 1;
					}
				}
			if(rd_count != 0)
				{		/* read a pixel and decode into RGB */
				switch (hdr.PixelSize)
					{
					case 8:	/* Pseudo or Gray */
						if(zread(zf, buf, 1) != 1)
							goto data_short;
					break;
			
					case 16:	/* 5 bits of RGB or 16 pseudo color */
					case 15:
						if(zread(zf, buf, 2) != 2)
							goto data_short;
						buf[2] = buf[0] & 0x1F;		/* B */
						buf[3] = ((buf[1] & 0x03) << 3) + ((unsigned)(buf[0] & 0xE0) >> 5);	/* G */
						buf[4] = (unsigned)(buf[1] & 0x7C) >> 2;	/* R */
					break;
			
					case 24:	/* 8 bits of B G R */
						if(zread(zf, &buf[2], 3) != 3)
							goto data_short;
					break;

					case 32:	/* 8 bits of B G R + alpha */
						if(zread(zf, &buf[2], 4) != 4)
							goto data_short;
					break;
					}
				rd_count--;
				}
			switch (image->pixlen)
				{
				case 1:					/* 8 bit pseudo or gray */
					*data++ = buf[0];
					break;
				case 2:					/* 16 bit pseudo */
					*data++ = buf[1];	/* tga is little endian, */
					*data++ = buf[0];	/* xli is big endian */
					break;
				case 3:					/* True color */
					*data++ = buf[4];	/* R */
					*data++ = buf[3];	/* G */
					*data++ = buf[2];	/* B */
					break;
				}
			wr_count--;
			}
		}
	zclose(zf);
	return image;

data_short:
	fprintf(stderr, "tgaLoad: %s - Short read within Data\n",hdr.name);
	zclose(zf);
	return image;
	}

