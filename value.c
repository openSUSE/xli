/* #ident	"@(#)x11:contrib/clients/xloadimage/value.c 6.9 93/07/23 Labtam" */
/* value.c:
 *
 * routines for converting byte values to long values.  these are pretty
 * portable although they are not necessarily the fastest things in the
 * world.
 *
 * jim frost 10.02.89
 *
 * Copyright 1989 Jim Frost.
 * See included file "copyright.h" for complete copyright information.
 */

#include "copyright.h"
#include "xli.h"

/* this flips all the bits in a byte array at byte intervals
 */

void flipBits(byte *p, unsigned int len)
{ static int init= 0;
  static byte flipped[256];

  if (!init) {
    int a, b;
    byte norm;

    for (a= 0; a < 256; a++) {
      flipped[a]= 0;
      norm= a;
      for (b= 0; b < 8; b++) {
	flipped[a]= (flipped[a] << 1) | (norm & 1);
	norm >>= 1;
      }
    }
  }

  while (len--)
    p[len]= flipped[p[len]];
}
