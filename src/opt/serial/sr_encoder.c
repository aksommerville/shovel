#include "serial.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>

/* Cleanup.
 */

void sr_encoder_cleanup(struct sr_encoder *encoder) {
  if (encoder->v) free(encoder->v);
}

/* Grow and assert.
 */

int sr_encoder_require(struct sr_encoder *encoder,int addc) {
  if (encoder->err) return -1;
  if (addc<=0) return 0;
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

/* Append raw data.
 */

int sr_encode_raw(struct sr_encoder *encoder,const void *src,int srcc) {
  if (encoder->err) return -1;
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (sr_encoder_require(encoder,srcc)<0) return -1;
  memcpy((char*)encoder->v+encoder->c,src,srcc);
  encoder->c+=srcc;
  return 0;
}

/* Append formatted text.
 */
 
int sr_encode_fmt(struct sr_encoder *encoder,const char *fmt,...) {
  if (encoder->err) return -1;
  if (!fmt) return 0;
  for (;;) {
    va_list vargs;
    va_start(vargs,fmt);
    int err=vsnprintf((char*)encoder->v+encoder->c,encoder->a-encoder->c,fmt,vargs);
    if ((err<0)||(err>=INT_MAX)) return encoder->err=-1;
    if (encoder->c<encoder->a-err) { // strictly <; vsnprintf emits a terminator
      encoder->c+=err;
      return 0;
    }
    if (sr_encoder_require(encoder,err+1)<0) return -1;
  }
}

/* Append zeroes.
 */
 
int sr_encode_zero(struct sr_encoder *encoder,int c) {
  if (encoder->err) return -1;
  if (c<1) return 0;
  if (sr_encoder_require(encoder,c)<0) return -1;
  memset((char*)encoder->v+encoder->c,0,c);
  encoder->c+=c;
  return 0;
}

/* Plain integers.
 */

int sr_encode_u8(struct sr_encoder *encoder,int v) {
  if (sr_encoder_require(encoder,1)<0) return -1;
  ((unsigned char*)encoder->v)[encoder->c++]=v;
  return 0;
}

int sr_encode_intle(struct sr_encoder *encoder,int v,int size) {
  if ((size<1)||(size>4)) return encoder->err=-1;
  if (sr_encoder_require(encoder,size)<0) return -1;
  unsigned char *dst=((unsigned char*)encoder->v)+encoder->c;
  int i=0; for (;i<size;i++,v>>=8) dst[i]=v;
  encoder->c+=size;
  return 0;
}

int sr_encode_intbe(struct sr_encoder *encoder,int v,int size) {
  if ((size<1)||(size>4)) return encoder->err=-1;
  if (sr_encoder_require(encoder,size)<0) return -1;
  unsigned char *dst=((unsigned char*)encoder->v)+encoder->c;
  int i=size; for (;i-->0;v>>=8) dst[i]=v;
  encoder->c+=size;
  return 0;
}
