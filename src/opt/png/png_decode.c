#include "png.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <zlib.h>

/* Context.
 */
 
struct png_decoder {
  const char *refname;
  struct png_image *image; // STRONG
  const uint8_t *src; // WEAK
  int srcc;
  int srcp;
  int err;
  int y;
  int xstride;
  uint8_t *rowbuf; // 1+image->stride; reception area from zlib
  z_stream z;
  int zinit;
};

static void png_decoder_cleanup(struct png_decoder *decoder) {
  png_image_del(decoder->image);
  if (decoder->rowbuf) free(decoder->rowbuf);
  if (decoder->zinit) inflateEnd(&decoder->z);
}

/* Log error.
 */
 
static int png_fail(struct png_decoder *decoder,const char *fmt,...) {
  if (decoder->err<0) return decoder->err;
  if (!decoder->refname) return decoder->err=-1;
  va_list vargs;
  va_start(vargs,fmt);
  char msg[256];
  int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
  if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
  while (msgc&&((unsigned char)msg[msgc-1]<=0x20)) msgc--; // In case i accidentally include a newline.
  fprintf(stderr,"%s: %.*s\n",decoder->refname,msgc,msg);
  return decoder->err=-2;
}

/* Receive IHDR.
 * During reception we will create the image and most of the rest of the context.
 * (decoder->image) should serve as our "IHDR OK" flag.
 */
 
static int png_decode_IHDR(struct png_decoder *decoder,const uint8_t *src,int srcc) {

  if (decoder->image||decoder->rowbuf||decoder->zinit) return png_fail(decoder,"Multiple IHDR.");
  if (srcc<13) return png_fail(decoder,"Unexpected IHDR length %d.",srcc);
  int w=(src[0]<<24)|(src[1]<<16)|(src[2]<<8)|src[3];
  int h=(src[4]<<24)|(src[5]<<16)|(src[6]<<8)|src[7];
  int depth=src[8];
  int colortype=src[9];
  if (src[12]==1) return png_fail(decoder,"Adam7 interlacing not supported."); // Spec says we must but meh.
  if (src[10]||src[11]||src[12]) return png_fail(decoder,"Unexpected compression, filter, or interlace strategy.");
  if (!(decoder->image=png_image_new(w,h,depth,colortype))) {
    return png_fail(decoder,"Failed to create image %dx%d depth=%d colortype=%d.",w,h,depth,colortype);
  }
  
  if ((decoder->xstride=decoder->image->pixelsize>>3)<1) decoder->xstride=1;
  if (!(decoder->rowbuf=malloc(1+decoder->image->stride))) return png_fail(decoder,"Out of memory.");
  if (inflateInit(&decoder->z)<0) return png_fail(decoder,"Failed to init zlib context.");
  decoder->zinit=1;
  decoder->z.next_out=(Bytef*)decoder->rowbuf;
  decoder->z.avail_out=1+decoder->image->stride;
  
  return 0;
}

/* Unfilter.
 */
 
static void png_unfilter_SUB(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int len,int xstride) {
  int i=0;
  for (;i<xstride;i++) dst[i]=src[i];
  for (;i<len;i++) dst[i]=src[i]+dst[i-xstride];
}

static void png_unfilter_UP(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int len,int xstride) {
  int i=0;
  if (prv) {
    for (;i<len;i++) dst[i]=src[i]+prv[i];
  } else {
    memcpy(dst,src,len);
  }
}

static void png_unfilter_AVG(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int len,int xstride) {
  int i=0;
  if (prv) {
    for (;i<xstride;i++) dst[i]=src[i]+(prv[i]>>1);
    for (;i<len;i++) dst[i]=src[i]+((prv[i]+dst[i-xstride])>>1);
  } else {
    for (;i<xstride;i++) dst[i]=src[i];
    for (;i<len;i++) dst[i]=src[i]+(dst[i-xstride]>>1);
  }
}

static uint8_t png_paeth(uint8_t a,uint8_t b,uint8_t c) {
  int p=a+b-c;
  int pa=p-a; if (pa<0) pa=-pa;
  int pb=p-b; if (pb<0) pb=-pb;
  int pc=p-c; if (pc<0) pc=-pc;
  if ((pa<=pb)&&(pa<=pc)) return a;
  if (pb<=pc) return b;
  return c;
}

static void png_unfilter_PAETH(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int len,int xstride) {
  int i=0;
  if (prv) {
    for (;i<xstride;i++) dst[i]=src[i]+png_paeth(0,prv[i],0);
    for (;i<len;i++) dst[i]=src[i]+png_paeth(dst[i-xstride],prv[i],prv[i-xstride]);
  } else {
    for (;i<xstride;i++) dst[i]=src[i];
    for (;i<len;i++) dst[i]=src[i]+dst[i-xstride];
  }
}

/* Unfilter (rowbuf) and copy into the image at (y).
 * Caller guarantees these are both valid.
 * Caller updates zlib state after.
 */
 
static int png_receive_row(struct png_decoder *decoder) {
  uint8_t *dst=((uint8_t*)decoder->image->v)+decoder->y*decoder->image->stride;
  uint8_t *prv=0;
  if (decoder->y) prv=dst-decoder->image->stride;
  switch (decoder->rowbuf[0]) {
    case 0: memcpy(dst,decoder->rowbuf+1,decoder->image->stride); break;
    case 1: png_unfilter_SUB(dst,decoder->rowbuf+1,prv,decoder->image->stride,decoder->xstride); break;
    case 2: png_unfilter_UP(dst,decoder->rowbuf+1,prv,decoder->image->stride,decoder->xstride); break;
    case 3: png_unfilter_AVG(dst,decoder->rowbuf+1,prv,decoder->image->stride,decoder->xstride); break;
    case 4: png_unfilter_PAETH(dst,decoder->rowbuf+1,prv,decoder->image->stride,decoder->xstride); break;
    default: return png_fail(decoder,"Unexpected filter byte 0x%02x at row %d/%d\n",decoder->rowbuf[0],decoder->y,decoder->image->h);
  }
  decoder->y++;
  return 0;
}

/* Receive IDAT.
 */
 
static int png_decode_IDAT(struct png_decoder *decoder,const uint8_t *src,int srcc) {
  int err;
  if (!decoder->zinit) return png_fail(decoder,"IHDR required before IDAT.");
  decoder->z.next_in=(Bytef*)src;
  decoder->z.avail_in=srcc;
  while (decoder->z.avail_in>0) {
    if (decoder->y>=decoder->image->h) break;
    if (!decoder->z.avail_out) {
      if ((err=png_receive_row(decoder))<0) return err;
      decoder->z.next_out=(Bytef*)decoder->rowbuf;
      decoder->z.avail_out=1+decoder->image->stride;
    }
    if ((err=inflate(&decoder->z,Z_NO_FLUSH))<0) return png_fail(decoder,"Error %d from inflate(). (%d/%d)",err,decoder->y,decoder->image->h);
  }
  return 0;
}

/* Assert completion.
 */
 
static int png_decode_finish(struct png_decoder *decoder) {
  int err,finished=0;
  if (!decoder->image) return png_fail(decoder,"No IHDR.");
  for (;;) {
    if (decoder->y>=decoder->image->h) break;
    if (!decoder->z.avail_out) {
      if ((err=png_receive_row(decoder))<0) return err;
      decoder->z.next_out=(Bytef*)decoder->rowbuf;
      decoder->z.avail_out=1+decoder->image->stride;
    }
    if (finished) break;
    if ((err=inflate(&decoder->z,Z_FINISH))<0) return png_fail(decoder,"Error %d from inflate(). (wrapping up)",err);
    if (err==Z_STREAM_END) finished=1;
  }
  if (decoder->y<decoder->image->h) return png_fail(decoder,"Incomplete image data.");
  return 0;
}

/* Main operation, in context.
 */
 
static int png_decode_inner(struct png_decoder *decoder) {
  if ((decoder->srcc<8)||memcmp(decoder->src,"\x89PNG\r\n\x1a\n",8)) return png_fail(decoder,"PNG signature mismatch.");
  decoder->srcp=8;
  int iended=0,err;
  while (!iended) {
    if (decoder->srcp>decoder->srcc-8) return png_fail(decoder,"Unexpected EOF.");
    int chunklen=decoder->src[decoder->srcp++]<<24;
    chunklen|=decoder->src[decoder->srcp++]<<16;
    chunklen|=decoder->src[decoder->srcp++]<<8;
    chunklen|=decoder->src[decoder->srcp++];
    int chunkid=decoder->src[decoder->srcp++]<<24;
    chunkid|=decoder->src[decoder->srcp++]<<16;
    chunkid|=decoder->src[decoder->srcp++]<<8;
    chunkid|=decoder->src[decoder->srcp++];
    if ((chunklen<0)||(decoder->srcp>decoder->srcc-chunklen)) return png_fail(decoder,"Invalid chunk length.");
    const void *chunk=decoder->src+decoder->srcp;
    decoder->srcp+=chunklen;
    if (decoder->srcp>decoder->srcc-4) return png_fail(decoder,"Unexpected EOF.");
    decoder->srcp+=4; // Don't bother checking CRCs, who cares.
    switch (chunkid) {
      case PNG_CHUNKID_IHDR: if ((err=png_decode_IHDR(decoder,chunk,chunklen))<0) return err; break;
      case PNG_CHUNKID_IDAT: if ((err=png_decode_IDAT(decoder,chunk,chunklen))<0) return err; break;
      case PNG_CHUNKID_IEND: iended=1; break;
      default: { // Everything else, just push it on the list.
          if (!decoder->image) return png_fail(decoder,"Expected IHDR.");
          if (!png_image_add_chunk(decoder->image,chunkid,chunk,chunklen)) return png_fail(decoder,"Failed to add chunk.");
        } break;
    }
  }
  png_decode_finish(decoder);
  return decoder->err;
}

/* Public entry point.
 */
 
struct png_image *png_decode(const void *src,int srcc,const char *refname) {
  if (!src) return 0;
  struct png_decoder decoder={
    .refname=refname,
    .src=src,
    .srcc=srcc,
  };
  int err=png_decode_inner(&decoder);
  if (err<0) {
    png_decoder_cleanup(&decoder);
    return 0;
  }
  struct png_image *image=decoder.image;
  decoder.image=0;
  png_decoder_cleanup(&decoder);
  return image;
}
