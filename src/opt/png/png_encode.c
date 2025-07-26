/* png_encode.c
 * I don't expect to actually use this; more likely we'll be decoding PNG and emitting something app-specific.
 * But for the sake of completeness...
 */
 
#include "png.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <zlib.h>

/* Context.
 */
 
struct png_encoder {
  uint8_t *v;
  int c,a;
  const struct png_image *image; // WEAK
  int err; // Sticky error.
  z_stream z;
  int zinit;
  uint8_t *rowbuf; // length is (1+image->stride).
  int xstride; // Depends on pixelsize, min 1.
};

static void png_encoder_cleanup(struct png_encoder *encoder) {
  if (encoder->v) free(encoder->v);
  if (encoder->zinit) deflateEnd(&encoder->z);
  if (encoder->rowbuf) free(encoder->rowbuf);
}

/* Append to encoder's output.
 */
 
static int png_encoder_require(struct png_encoder *encoder,int addc) {
  if (encoder->err<0) return encoder->err;
  if (addc<1) return 0;
  if (encoder->c>INT_MAX-addc) return encoder->err=-1;
  int na=encoder->c+addc;
  if (na<=encoder->a) return 0;
  if (na<INT_MAX-1024) na=(na+1024)&~1023;
  void *nv=realloc(encoder->v,na);
  if (!nv) return encoder->err=-1;
  encoder->v=nv;
  encoder->a=na;
  return 0;
}
 
static int png_encode_raw(struct png_encoder *encoder,const void *v,int c) {
  if (!v) return 0;
  if (c<0) { c=0; while (((char*)v)[c]) c++; }
  if (png_encoder_require(encoder,c)<0) return -1;
  memcpy(encoder->v+encoder->c,v,c);
  encoder->c+=c;
  return 0;
}

static int png_encode_u32(struct png_encoder *encoder,int v) {
  uint8_t tmp[]={v>>24,v>>16,v>>8,v};
  return png_encode_raw(encoder,tmp,sizeof(tmp));
}

static int png_encode_u8(struct png_encoder *encoder,int v) {
  uint8_t tmp=v;
  return png_encode_raw(encoder,&tmp,1);
}

// Emit CRC for a chunk with nominal length (len). Must also contain the 4-byte chunk id before that.
static int png_encode_crc(struct png_encoder *encoder,int len) {
  if ((len<0)||(len>INT_MAX-4)) return encoder->err=-1;
  len+=4;
  if (len>encoder->c) return encoder->err=-1;
  int crc=crc32(crc32(0,0,0),encoder->v+encoder->c-len,len);
  return png_encode_u32(encoder,crc);
}

// Given the start of a chunk's length field, fill in length and then append CRC.
static int png_encode_finish_chunk(struct png_encoder *encoder,int lenp) {
  int len=encoder->c-lenp-8;
  if (len<0) return -1;
  encoder->v[lenp+0]=len>>24;
  encoder->v[lenp+1]=len>>16;
  encoder->v[lenp+2]=len>>8;
  encoder->v[lenp+3]=len;
  return png_encode_crc(encoder,len);
}

/* Perform row filters.
 * For these, (dst) must not include the filter byte; filter byte is the caller's problem.
 * Those that take (prv), it's not allowed to be null.
 */
 
static void png_filter_SUB(uint8_t *dst,const uint8_t *src,int len,int xstride) {
  int i=0;
  for (;i<xstride;i++) dst[i]=src[i];
  for (;i<len;i++) dst[i]=src[i]-src[i-xstride];
}

static void png_filter_UP(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int len) {
  int i=0;
  for (;i<len;i++) dst[i]=src[i]-prv[i];
}

static void png_filter_AVG(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int len,int xstride) {
  int i=0;
  for (;i<xstride;i++) dst[i]=src[i]-(prv[i]>>1);
  for (;i<len;i++) dst[i]=src[i]-((src[i-xstride]+prv[i])>>1);
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

static void png_filter_PAETH(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int len,int xstride) {
  int i=0;
  for (;i<xstride;i++) dst[i]=src[i]-png_paeth(0,prv[i],0);
  for (;i<len;i++) dst[i]=src[i]-png_paeth(src[i-xstride],prv[i],prv[i-xstride]);
}

static int png_count_zeroes(const uint8_t *v,int c) {
  int zeroc=0;
  for (;c-->0;v++) if (!*v) zeroc++;
  return zeroc;
}

/* Filter one row of image data.
 * (prv) may be null, for the first row.
 * (dst)'s length is (len+1); it starts with a filter identifier.
 */
 
static void png_encoder_filter_row(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int len,int xstride) {
  /* Without (prv) try only NONE and SUB.
   * With (prv), also try UP, AVG, and PAETH.
   * This way, the 3 filters that use (prv) may assume that it exists. (spec allows them in any case).
   */
  int best=0,q;
  memcpy(dst+1,src,len);
  int zeroc=png_count_zeroes(dst,len);
  png_filter_SUB(dst+1,src,len,xstride);
  if ((q=png_count_zeroes(dst+1,len))>zeroc) { best=1; zeroc=q; }
  if (prv) {
    png_filter_UP(dst+1,src,prv,len);
    if ((q=png_count_zeroes(dst+1,len))>zeroc) { best=2; zeroc=q; }
    png_filter_AVG(dst+1,src,prv,len,xstride);
    if ((q=png_count_zeroes(dst+1,len))>zeroc) { best=3; zeroc=q; }
    png_filter_PAETH(dst+1,src,prv,len,xstride);
    if ((q=png_count_zeroes(dst+1,len))>zeroc) { best=4; zeroc=q; }
  }
  switch (dst[0]=best) {
    case 0: memcpy(dst+1,src,len); break;
    case 1: png_filter_SUB(dst+1,src,len,xstride); break;
    case 2: png_filter_UP(dst+1,src,prv,len); break;
    case 3: png_filter_AVG(dst+1,src,prv,len,xstride); break;
    case 4: break; // Must already have PAETH there, it's the last.
  }
}

/* Encode IDAT chunk, no framing.
 * We are allowed to break the pixels into multiple IDATs, but we always do just one.
 */
 
static int png_encode_idat(struct png_encoder *encoder) {
  if (encoder->zinit||encoder->rowbuf) return encoder->err=-1;
  if (deflateInit(&encoder->z,Z_BEST_COMPRESSION)<0) return encoder->err=-1;
  encoder->zinit=1;
  if (!(encoder->rowbuf=malloc(1+encoder->image->stride))) return encoder->err=-1;
  if ((encoder->xstride=encoder->image->pixelsize>>3)<1) encoder->xstride=1;
  const uint8_t *src=encoder->v,*prv=0;
  int y=0;
  for (;y<encoder->image->h;y++,prv=src,src+=encoder->image->stride) {
  
    // Queue up the next row in (rowbuf) and set zlib's input to that.
    png_encoder_filter_row(encoder->rowbuf,src,prv,encoder->image->stride,encoder->xstride);
    encoder->z.next_in=(Bytef*)encoder->rowbuf;
    encoder->z.avail_in=1+encoder->image->stride;
    
    // Pump zlib until it consumes all input, growing output as needed.
    while (encoder->z.avail_in) {
      if (encoder->z.avail_out<1) {
        if (png_encoder_require(encoder,8192)<0) return -1;
        encoder->z.next_out=(Bytef*)(encoder->v+encoder->c);
        encoder->z.avail_out=encoder->a-encoder->c;
      }
      int ao0=encoder->z.avail_out;
      int err=deflate(&encoder->z,Z_NO_FLUSH);
      if (err<0) return encoder->err=-1;
      int addc=ao0-encoder->z.avail_out;
      if (addc>0) encoder->c+=addc;
    }
  }
  // Pump zlib until it finishes.
  for (;;) {
    if (encoder->z.avail_out<1) {
      if (png_encoder_require(encoder,8192)<0) return -1;
      encoder->z.next_out=(Bytef*)(encoder->v+encoder->c);
      encoder->z.avail_out=encoder->a-encoder->c;
    }
    int ao0=encoder->z.avail_out;
    int err=deflate(&encoder->z,Z_FINISH);
    if (err<0) return encoder->err=-1;
    int addc=ao0-encoder->z.avail_out;
    if (addc>0) encoder->c+=addc;
    if (err==Z_STREAM_END) break;
  }
  return 0;
}

/* Main operation, in context.
 */
 
static int png_encode_inner(struct png_encoder *encoder) {

  // Signature and IHDR.
  png_encode_raw(encoder,"\x89PNG\r\n\x1a\n",8);
  png_encode_u32(encoder,13);
  png_encode_u32(encoder,PNG_CHUNKID_IHDR);
  png_encode_u32(encoder,encoder->image->w);
  png_encode_u32(encoder,encoder->image->h);
  png_encode_u8(encoder,encoder->image->depth);
  png_encode_u8(encoder,encoder->image->colortype);
  png_encode_raw(encoder,"\0\0\0",3); // Compression, Filter, Interlace: We don't support Adam7.
  png_encode_crc(encoder,13);
  
  // Extra chunks before IDAT. PLTE and tRNS are required to come before, others are merely allowed to.
  const struct png_chunk *chunk=encoder->image->chunkv;
  int i=encoder->image->chunkc;
  for (;i-->0;chunk++) {
    png_encode_u32(encoder,chunk->c);
    png_encode_u32(encoder,chunk->id);
    png_encode_raw(encoder,chunk->v,chunk->c);
    png_encode_crc(encoder,chunk->c);
  }
  
  // IDAT.
  int idatp=encoder->c;
  png_encode_u32(encoder,0); // Placeholder for length.
  png_encode_u32(encoder,PNG_CHUNKID_IDAT);
  if (png_encode_idat(encoder)<0) return encoder->err=-1;
  png_encode_finish_chunk(encoder,idatp);
  
  // IEND, and check the sticky error.
  png_encode_u32(encoder,0);
  png_encode_u32(encoder,PNG_CHUNKID_IEND);
  png_encode_u32(encoder,0xae426082);
  return encoder->err;
}

/* Main entry point.
 */
 
int png_encode(void *dstpp,const struct png_image *image) {
  if (!dstpp||!image) return -1;
  struct png_encoder encoder={
    .image=image,
  };
  int err=png_encode_inner(&encoder);
  if (err<0) {
    png_encoder_cleanup(&encoder);
    return -1;
  }
  *(void**)dstpp=encoder.v;
  encoder.v=0;
  err=encoder.c;
  png_encoder_cleanup(&encoder);
  return err;
}
