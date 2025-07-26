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
  
  // Determine object name from (dstpath), or (path), or make one up.
  const char *name=0;
  int namec=0;
  if (dstpath) {
    int pathp=0,stop=0;
    for (;dstpath[pathp];pathp++) {
      if (dstpath[pathp]=='/') {
        name=dstpath+pathp+1;
        namec=0;
        stop=0;
      } else if (dstpath[pathp]=='.') {
        stop=1;
      } else if (name&&!stop) {
        namec++;
      }
    }
  }
  if (path&&!namec) {
    int pathp=0,stop=0;
    for (;path[pathp];pathp++) {
      if (path[pathp]=='/') {
        name=path+pathp+1;
        namec=0;
        stop=0;
      } else if (path[pathp]=='.') {
        stop=1;
      } else if (name&&!stop) {
        namec++;
      }
    }
  }
  if (!namec) {
    name="image";
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
