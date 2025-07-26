/* png.h
 * Simple PNG decoder and encoder.
 * We do not support interlacing.
 * We support tRNS for indexed pixels but not other formats (where it's supposed to be a colorkey).
 * Requires libc and zlib.
 */
 
#ifndef PNG_H
#define PNG_H

#include <stdint.h>

/* Image object.
 ********************************************************************/

struct png_image {
  void *v;
  int w,h;
  int stride; // bytes
  int depth,colortype; // as encoded
  int pixelsize; // bits
  
  /* IHDR,IDAT,IEND are never included here.
   * All others are, and their order in the file is preserved.
   */
  struct png_chunk {
    uint32_t id; // big-endian
    void *v;
    int c;
  } *chunkv;
  int chunkc,chunka;
};

/* It is perfectly safe to yoink (image->v) and delete the rest.
 */
void png_image_del(struct png_image *image);

/* Allocates and zeroes pixels. Minimum stride. No extra chunks.
 * Invalid (depth,colortype) combinations are permitted if they yield a valid pixel size: 1,2,4,8,16,24,32,48,64
 */
struct png_image *png_image_new(int w,int h,int depth,int colortype);

/* Copies (v).
 */
struct png_chunk *png_image_add_chunk(struct png_image *image,uint32_t id,const void *v,int c);

void png_image_remove_chunk_at(struct png_image *image,int p);

/* Replace or add PLTE and tRNS chunks.
 * (image->colortype) should be 3, but we don't verify.
 * (chanc) must be 3 if (v) contains [r,g,b] or 4 if [r,g,b,a].
 * (c) is the count of entries, not bytes.
 */
int png_image_set_ctab(struct png_image *image,const void *v,int c,int chanc);

/* Encoding and decoding are strictly one-shot, to keep the API simple.
 ************************************************************************/

/* If you provide (refname), we'll log all errors to stderr, and possibly warnings too.
 * Otherwise, no logging.
 */
struct png_image *png_decode(const void *src,int srcc,const char *refname);

/* On success (*dstpp) points to a newly-allocated buffer which you must free.
 * Returns its length.
 */
int png_encode(void *dstpp,const struct png_image *image);

#define PNG_CHUNKID_IHDR 0x49484452
#define PNG_CHUNKID_IDAT 0x49444154
#define PNG_CHUNKID_IEND 0x49454e44
#define PNG_CHUNKID_PLTE 0x504c5445
#define PNG_CHUNKID_tRNS 0x74524e53

/* Reformat.
 *************************************************************************/

/* Make a new image with minimum stride and the requested format.
 * To or from indexed pixels, we use (src)'s PLTE and tRNS if present.
 * Those conversions, especially true-to-indexed, are outrageously expensive.
 */
struct png_image *png_image_reformat(const struct png_image *src,int depth,int colortype);

/* Reformat, with the destination image created.
 * If you want to use a new color table, you have to do this instead of png_image_reformat().
 * (dst) and (src) must have the same dimensions.
 * This is just a structured use of png_iterate().
 */
int png_image_convert(struct png_image *dst,const struct png_image *src);

/* Prepare an iterator that you can use to visit each of an image's pixels in scan order.
 * On success, iter->(next,read,write) will all be populated, and (read,write) will address the first pixel.
 * Since they all take 64-bit RGBA, reading and writing to indexed images is expensive.
 * All of the formats stipulated by the spec are supported. Note that png_image_new() permits others, which will fail here.
 */
struct png_pixel {
  uint16_t r,g,b,a;
};
struct png_iterator {
  int (*next)(struct png_iterator *iter); // => 0 if finished
  struct png_pixel (*read)(struct png_iterator *iter);
  void (*write)(struct png_iterator *iter,struct png_pixel pixel);
  struct png_image *image; // WEAK
  uint8_t *rowstart;
  uint8_t *p;
  int xc,yc;
  int shift;
};
int png_iterate(struct png_iterator *iter,struct png_image *image);

#endif
