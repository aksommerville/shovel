/* r1b.h
 * 1-bit renderer.
 * We produce 32-bit RGBX as output, but all source images are 1-bit.
 * We assume little-endian for the 32-bit pixels (0x000000ff=red and 0xff000000=x).
 * Source images are row-padded to at least one byte, and bits are arranged big-endianly like PNG.
 */
 
#ifndef R1B_H
#define R1B_H

// No "SWAP" transform. It's complicated and doesn't come up that often.
#define R1B_XFORM_XREV 1
#define R1B_XFORM_YREV 2

struct r1b_img32 {
  unsigned int *v; // xbgr, ie red is stored first
  int w,h;
  int stridewords; // Stride in pixels, usually (w).
};

struct r1b_img1 {
  unsigned char *v;
  int w,h;
  int stride; // Stride in bytes.
};

/* In coordination with our build tool, use these macros to declare an image file and assign it to an r1b_img1.
 */
#define R1B_DECL(name) \
  extern const int name##_w,name##_h,name##_stride,name##_pixelsize; \
  extern const unsigned char name##_pixels[];
#define R1B_ASSIGN(name) \
  if (name##_pixelsize==1) { \
    name##_image.w=name##_w; \
    name##_image.h=name##_h; \
    name##_image.stride=name##_stride; \
    name##_image.v=(void*)name##_pixels; \
  }

/* Solid rectangle directly onto a 32-bit image.
 */
void r1b_img32_fill_rect(struct r1b_img32 *dst,int x,int y,int w,int h,unsigned int xbgr);

//TODO Line? Oval? Triangle? Curve?
//TODO Gradients? Patterns?

/* Blit a 1-bit image onto a 32-bit image.
 * This should be the main way you draw things.
 * You provide two colors. If either is natural zero, it's transparent. (use a non-zero high byte for pure black).
 * You may flip on one or both axes. This doesn't change the covered area.
 * We crop edges for you.
 */
void r1b_img32_blit_img1(
  struct r1b_img32 *dst,
  const struct r1b_img1 *src,
  int dstx,int dsty,
  int srcx,int srcy,
  int w,int h,
  unsigned int xbgr0,unsigned int xbgr1,
  unsigned char xform
);

//TODO Will there be a need for 1-bit-to-1-bit blits? Those are complicated; don't implement unless we need them.

#endif
