/* mcidas.c:
 *
 * McIDAS areafile support.  contributed by Glenn P. Davis
 * (davis@unidata.ucar.edu).
 */

#include "xli.h"
#include "imagetypes.h"
#include "mcidas.h"

char *mc_sensor(int sscode);

/*
 * convert from little endian to big endian four byte object
 */
static unsigned long
vhtonl(long unsigned int lend)
{
	unsigned long bend ;
	unsigned char *lp, *bp ;

	lp = ((unsigned char *)&lend) + 3 ;
	bp = (unsigned char *) &bend ;

	*bp++ = *lp-- ;
	*bp++ = *lp-- ;
	*bp++ = *lp-- ;
	*bp = *lp ;

	return(bend) ;
}


/* ARGSUSED */
int mcidasIdent(char *fullname, char *name)
{ ZFILE          *zf;
  struct area_dir dir ;
  int             r;
  int doswap = 0 ;

  if (! (zf= zopen(fullname))) {
    perror("mcidasIdent");
    return(0);
  }
  switch (zread(zf, (byte *)&dir, sizeof(struct area_dir))) {

  case sizeof(struct area_dir):
    if (dir.type == 4) {
      r= 1;
      break;
    }
    else if (dir.type == 67108864) {
      r= 1;
      doswap = 1 ;
    } else {
      r= 0;
      break;
    }

  case -1:
    perror("mcidasIdent");
  default:
    r= 0;
    break;
  }
  zclose(zf);
  if(r && doswap) {
    unsigned long *begin ; 
    unsigned long *ulp ;
    begin = (unsigned long *)&dir ;
    for(ulp = begin ; ulp < &begin[AREA_COMMENTS] ; ulp++)
       *ulp = vhtonl(*ulp) ;
    for(ulp = &begin[AREA_CALKEY] ; ulp < &begin[AREA_STYPE] ; ulp++)
       *ulp = vhtonl(*ulp) ;
  }

  /* a simple sanity check */
  if (dir.idate > 99999 || dir.itime > 999999)
    r = 0;
	
  if (r) {
   (void)printf("%s %ld %ld (%ld, %ld) (%ld, %ld)\n",
		 mc_sensor(dir.satid),
		 dir.idate,
		 dir.itime,
		 dir.lcor,
		 dir.ecor,
		 dir.lres,
		 dir.eres) ;
  }
  return(r);
}

Image *mcidasLoad(char *fullname, ImageOptions *image_ops, boolean verbose)
{ ZFILE        *zf;
  char         *name = image_ops->name;
  struct area_dir  dir;
  struct navigation  nav;
  Image          *image;
  unsigned int    y;
  int doswap = 0 ;

  if (! (zf= zopen(fullname))) {
    perror("mcidasLoad");
    return(NULL);
  }
  switch (zread(zf, (byte *)&dir, sizeof(struct area_dir))) {

  case sizeof(struct area_dir):
    if (dir.type != 4) {
      if(dir.type != 67108864) {
        zclose(zf);
        return(NULL) ;
      } else {
	doswap = 1 ;
      }
    }
    break;

  case -1:
    perror("mcidasLoad");
  default:
    zclose(zf);
    return(NULL);
  }

  if(doswap) {
    unsigned long *begin ; 
    unsigned long *ulp ;
    begin = (unsigned long *)&dir ;
    for(ulp = begin ; ulp < &begin[AREA_COMMENTS] ; ulp++)
       *ulp = vhtonl(*ulp) ;
     for(ulp = &begin[AREA_CALKEY] ; ulp < &begin[AREA_STYPE] ; ulp++)
        *ulp = vhtonl(*ulp) ;
   }

  /* a simple sanity check */
  if (dir.idate > 99999 || dir.itime > 999999) {
    zclose(zf);
    return(NULL);
  }
	
  if (verbose) {
    printf("%s %ld %ld (%ld, %ld) (%ld, %ld)\n",
		 mc_sensor(dir.satid),
		 dir.idate,
		 dir.itime,
		 dir.lcor,
		 dir.ecor,
		 dir.lres,
		 dir.eres);
  }

  znocache(zf);
  /* skip the nav */
  if( zread(zf, (byte *)&nav, sizeof(struct navigation)) !=
     sizeof(struct navigation)) {
      zclose(zf);
      return(NULL) ;
  }

  /* get an image to put the data in
   */

   image= newRGBImage(dir.esiz,
		       dir.lsiz,
		       8 * dir.bands);
  image->title= dupString(name);

  /* set up the colormap, linear grey scale
   */

    for (y= 0; y < 255; y++) {
      *(image->rgb.red + y)= 
       *(image->rgb.green + y)=
        *(image->rgb.blue + y)= y * 257 ;
    }
    image->rgb.used= 255 ;

  if (zread(zf, image->data, dir.esiz * dir.lsiz) != dir.esiz * dir.lsiz) {
    fprintf(stderr,"mcidasLoad: %s - Short read within image data\n",name);
    zclose(zf);
    return(image);
  }

  zclose(zf);
  read_trail_opt(image_ops,zf,image,verbose);
  return(image);
}
