#include "tool_internal.h"
#include "opt/png/png.h"

#define FAIL(fmt,...) { \
  if (!path) return -1; \
  fprintf(stderr,"%s: "fmt"\n",path,##__VA_ARGS__); \
  return -2; \
}

/* PNG from PNG.
 */
 
int tool_convert_png(struct sr_encoder *dst,const void *src,int srcc,const char *path) {
  // Easy to imagine adding options in the future like forcing a given pixel format.
  return sr_encode_raw(dst,src,srcc);
}

/* C from PNG.
 */
 
int tool_convert_c_png(struct sr_encoder *dst,const void *src,int srcc,const char *path,const char *dstpath) {

  // Decode PNG.
  struct png_image *image=png_decode(src,srcc,path);
  if (!image) return path?-2:-1;
  
  // Convert if requested. GIMP has this obnoxious habit of saving 1-bit images as 8-bit, so we'll do this a lot.
  int depth=tool_arg_int("depth",5,-1);
  int colortype=tool_arg_int("colortype",9,-1);
  if ((depth>=0)||(colortype>=0)) {
    if (depth<0) depth=image->depth;
    if (colortype<0) colortype=image->colortype;
    struct png_image *converted=png_image_reformat(image,depth,colortype);
    if (!converted) {
      png_image_del(image);
      FAIL("Failed to convert to depth=%d colortype=%d.",depth,colortype)
    }
    png_image_del(image);
    image=converted;
  }
  
  // Determine object name from (dstpath), or (path), or make one up.
  char name[64];
  int namec=tool_c_object_name(name,sizeof(name),path,dstpath);
  if ((namec<1)||(namec>sizeof(name))) {
    memcpy(name,"image",5);
    namec=5;
  }
  
  // Emit some metadata and begin the payload. Use PNG's natural formatting, why not.
  // stride and pixelsize are knowable from (w,depth,colortype) but include for convenience and validation.
  sr_encode_fmt(dst,"const int %.*s_w=%d;\n",namec,name,image->w);
  sr_encode_fmt(dst,"const int %.*s_h=%d;\n",namec,name,image->h);
  sr_encode_fmt(dst,"const int %.*s_stride=%d;\n",namec,name,image->stride);
  sr_encode_fmt(dst,"const int %.*s_depth=%d;\n",namec,name,image->depth);
  sr_encode_fmt(dst,"const int %.*s_colortype=%d;\n",namec,name,image->colortype);
  sr_encode_fmt(dst,"const int %.*s_pixelsize=%d;\n",namec,name,image->pixelsize);
  sr_encode_fmt(dst,"const unsigned char %.*s_pixels[]={\n",namec,name);
  
  // Emit pixels bytewise as decimal integers, and break lines every 100 bytes or so.
  int linestart=dst->c;
  const uint8_t *p=image->v;
  int c=image->stride*image->h;
  for (;c-->0;p++) {
    sr_encode_fmt(dst,"%d,",*p);
    if (dst->c-linestart>=100) {
      sr_encode_u8(dst,0x0a);
      linestart=dst->c;
    }
  }
  
  sr_encode_raw(dst,"};\n",3);
  png_image_del(image);
  return sr_encoder_require(dst,0);
}

/* Rawimg from PNG.
 */
 
int tool_convert_rawimg_png(struct sr_encoder *dst,const void *src,int srcc,const char *path,const char *dstpath) {

  char pixfmt[16];
  int chsize=0;
  int pixfmtc=tool_split_rawimg_fmt(pixfmt,sizeof(pixfmt),&chsize,dstpath);
  if ((pixfmtc<1)||(pixfmtc>sizeof(pixfmt))) FAIL("Unknown output format.")
  int colortype;
       if ((pixfmtc==1)&&!memcmp(pixfmt,"y",1)) colortype=0;
  else if ((pixfmtc==1)&&!memcmp(pixfmt,"a",1)) colortype=0;
  else if ((pixfmtc==1)&&!memcmp(pixfmt,"i",1)) colortype=3;
  else if ((pixfmtc==2)&&!memcmp(pixfmt,"ya",2)) colortype=4;
  else if ((pixfmtc==3)&&!memcmp(pixfmt,"rgb",3)) colortype=2;
  else if ((pixfmtc==4)&&!memcmp(pixfmt,"rgba",4)) colortype=6;
  else FAIL("Unknown output format.")

  struct png_image *image=png_decode(src,srcc,path);
  if (!image) return path?-2:-1;
  struct png_image *converted=png_image_reformat(image,chsize,colortype);
  png_image_del(image);
  if (!converted) FAIL("Failed to convert. Is the output format PNG-legal?")
  image=converted;

  int err=sr_encode_raw(dst,image->v,image->stride*image->h);
  png_image_del(image);
  return err;
}
