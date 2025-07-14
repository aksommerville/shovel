/* serial.h
 */
 
#ifndef SERIAL_H
#define SERIAL_H

/* Encoder.
 * Initialize to zeroes.
 * Cleanup not necessary if you yoink (v), otherwise cleanup mandatory if any calls made.
 *********************************************************************/
 
struct sr_encoder {
  void *v;
  int c,a;
  int err;
};

void sr_encoder_cleanup(struct sr_encoder *encoder);

/* Grow buffer without writing to it.
 * If (addc<=0), this only asserts that no sticky error is present.
 */
int sr_encoder_require(struct sr_encoder *encoder,int addc);

int sr_encode_raw(struct sr_encoder *encoder,const void *src,int srcc); // (srcc<0) if NUL-terminated
int sr_encode_fmt(struct sr_encoder *encoder,const char *fmt,...); // printf
int sr_encode_zero(struct sr_encoder *encoder,int c);

int sr_encode_u8(struct sr_encoder *encoder,int v);
int sr_encode_intle(struct sr_encoder *encoder,int v,int size); // size in bytes
int sr_encode_intbe(struct sr_encoder *encoder,int v,int size); // size in bytes

#endif
