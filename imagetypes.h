/* imagetypes.h:
 *
 * supported image types and the imagetypes array declaration.  when you
 * add a new image type, only the makefile and this header need to be
 * changed.
 *
 * jim frost 10.15.89
 */

Image *facesLoad(char *fullname, ImageOptions * image_ops, boolean verbose);
Image *pbmLoad(char *fullname, ImageOptions * image_ops, boolean verbose);
Image *sunRasterLoad(char *fullname, ImageOptions * image_ops,
	boolean verbose);
Image *gifLoad(char *fullname, ImageOptions * image_ops, boolean verbose);
Image *jpegLoad(char *fullname, ImageOptions * image_ops, boolean verbose);
Image *rleLoad(char *fullname, ImageOptions * image_ops, boolean verbose);
Image *bmpLoad(char *fullname, ImageOptions * image_ops, boolean verbose);
Image *pcdLoad(char *fullname, ImageOptions * image_ops, boolean verbose);
Image *xwdLoad(char *fullname, ImageOptions * image_ops, boolean verbose);
Image *xbitmapLoad(char *fullname, ImageOptions * image_ops, boolean verbose);
Image *xpixmapLoad(char *fullname, ImageOptions * image_ops, boolean verbose);
Image *g3Load(char *fullname, ImageOptions * image_ops, boolean verbose);
Image *fbmLoad(char *fullname, ImageOptions * image_ops, boolean verbose);
Image *pcxLoad(char *fullname, ImageOptions * image_ops, boolean verbose);
Image *imgLoad(char *fullname, ImageOptions * image_ops, boolean verbose);
Image *macLoad(char *fullname, ImageOptions * image_ops, boolean verbose);
Image *cmuwmLoad(char *fullname, ImageOptions * image_ops, boolean verbose);
Image *mcidasLoad(char *fullname, ImageOptions * image_ops, boolean verbose);
Image *tgaLoad(char *fullname, ImageOptions * image_ops, boolean verbose);
Image *pngLoad(char *fullname, ImageOptions * opt, boolean verbose);

int facesIdent(char *fullname, char *name);
int pbmIdent(char *fullname, char *name);
int sunRasterIdent(char *fullname, char *name);
int gifIdent(char *fullname, char *name);
int jpegIdent(char *fullname, char *name);
int rleIdent(char *fullname, char *name);
int bmpIdent(char *fullname, char *name);
int pcdIdent(char *fullname, char *name);
int xwdIdent(char *fullname, char *name);
int xbitmapIdent(char *fullname, char *name);
int xpixmapIdent(char *fullname, char *name);
int g3Ident(char *fullname, char *name);
int fbmIdent(char *fullname, char *name);
int pcxIdent(char *fullname, char *name);
int imgIdent(char *fullname, char *name);
int macIdent(char *fullname, char *name);
int cmuwmIdent(char *fullname, char *name);
int mcidasIdent(char *fullname, char *name);
int tgaIdent(char *fullname, char *name);
int pngIdent(char *fullname, char *name);
