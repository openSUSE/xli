/* #ident	"@(#)x11:contrib/clients/xloadimage/cmuwmrast.h 1.4 93/07/23 Labtam" */
/* cmuwmrast.h
 *
 * this describes the header for ITC (CMU WM) raster files. It is
 * essentially a byte reversed Sun raster, 1 plane, no encoding.
 */

#include "copyright.h"

struct cmuwm_header
{
    byte magic[4];
    byte width[4];
    byte height[4];
    byte depth[2];
};

#define CMUWM_MAGIC 0xf10040bb
