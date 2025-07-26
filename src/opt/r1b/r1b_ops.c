#include "r1b_internal.h"

/* Fill rect.
 */
 
void r1b_img32_fill_rect(struct r1b_img32 *dst,int x,int y,int w,int h,unsigned int xbgr) {
  if (!dst||!xbgr) return;
  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (x>dst->w-w) w=dst->w-x;
  if (y>dst->h-h) h=dst->h-y;
  if ((w<1)||(h<1)) return;
  unsigned int *dstrow=dst->v+y*dst->stridewords+x;
  for (;h-->0;dstrow+=dst->stridewords) {
    unsigned int *dstp=dstrow;
    int xi=w;
    for (;xi-->0;dstp++) *dstp=xbgr;
  }
}

/* Blit to 32 from 1.
 */

void r1b_img32_blit_img1(
  struct r1b_img32 *dst,
  const struct r1b_img1 *src,
  int dstx,int dsty,
  int srcx,int srcy,
  int w,int h,
  unsigned int xbgr0,unsigned int xbgr1,
  unsigned char xform
) {
  if (!dst||!src) return;
  if (!xbgr0&&!xbgr1) return; // Both colors transparent; automatically noop.
  
  // Clip both bounds to their edges, then get out if empty.
  if (dstx<0) {
    if (!(xform&R1B_XFORM_XREV)) srcx-=dstx;
    w+=dstx;
    dstx=0;
  }
  if (dsty<0) {
    if (!(xform&R1B_XFORM_YREV)) srcy-=dsty;
    h+=dsty;
    dsty=0;
  }
  if (dstx>dst->w-w) {
    if (xform&R1B_XFORM_XREV) srcx+=dstx+w-dst->w;
    w=dst->w-dstx;
  }
  if (dsty>dst->h-h) {
    if (xform&R1B_XFORM_YREV) srcy+=dsty+h-dst->h;
    h=dst->h-dsty;
  }
  if (srcx<0) {
    if (!(xform&R1B_XFORM_XREV)) dstx-=srcx;
    w+=srcx;
    srcx=0;
  }
  if (srcy<0) {
    if (!(xform&R1B_XFORM_YREV)) dsty-=srcy;
    h+=srcy;
    srcy=0;
  }
  if (srcx>src->w-w) {
    if (xform&R1B_XFORM_XREV) dstx+=srcx+w-src->w;
    w=src->w-srcx;
  }
  if (srcy>src->h-h) {
    if (xform&R1B_XFORM_YREV) dsty+=srcy+h-src->h;
    h=src->h-srcy;
  }
  if ((w<1)||(h<1)) return;
  
  /* We'll always read (src) LRTB, since bitwise reading is a bit complicated.
   * Effect the transform on the (dst) side.
   * From here, supporting a SWAP transform would be super simple. But the clipping above would be a nightmare.
   */
  int dminor=1,dmajor=dst->stridewords;
  if (xform&R1B_XFORM_XREV) {
    dminor=-1;
    dstx+=w-1;
  }
  if (xform&R1B_XFORM_YREV) {
    dmajor=-dst->stridewords;
    dsty+=h-1;
  }
  unsigned int *dstrow=dst->v+dsty*dst->stridewords+dstx;
  const unsigned char *srcrow=src->v+srcy*src->stride+(srcx>>3);
  unsigned char srcmask0=0x80>>(srcx&7);
  int yi=h;
  for (;yi-->0;dstrow+=dmajor,srcrow+=src->stride) {
    unsigned int *dstp=dstrow;
    const unsigned char *srcp=srcrow;
    unsigned char srcmask=srcmask0;
    int xi=w;
    for (;xi-->0;dstp+=dminor) {
      if ((*srcp)&srcmask) {
        if (xbgr1) *dstp=xbgr1;
      } else {
        if (xbgr0) *dstp=xbgr0;
      }
      if (srcmask==0x01) {
        srcmask=0x80;
        srcp++;
      } else {
        srcmask>>=1;
      }
    }
  }
}
