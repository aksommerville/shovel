/* audio/main.c
 *
 * Message format. Each message is identified by its leading byte.
 *   0x01 NOTE: [0x01,hz msb,hz lsb,level,duration 4ms]
 *   0x02 CHIRP: [0x02,hza msb,hza lsb,hzz msb,hzz lsb,level,duration 4ms]
 */

#include "shovel/shovel.h"
#include "opt/synmin/synmin.h"

static float buffer[1024];
 
int sha_init(int rate,int chanc) {
  if (synmin_init(rate,chanc)<0) return -1;
  sh_spcm(0,buffer,sizeof(buffer)/sizeof(buffer[0]));
  return 0;
}
 
void sha_update(int framec) {
  unsigned char msg[256];
  int msgc;
  while ((msgc=sh_mr(msg,sizeof(msg)))>0) {
    switch (msg[0]) {
      case 0x01: if (msgc>=5) {
          int hz=(msg[1]<<8)|msg[2];
          float level=msg[3]/255.0f;
          int durms=msg[4]<<2;
          synmin_note(hz,hz,level,durms);
        } break;
      case 0x02: if (msgc>=7) {
          int hza=(msg[1]<<8)|msg[2];
          int hzz=(msg[3]<<8)|msg[4];
          float level=msg[5]/255.0f;
          int durms=msg[6]<<2;
          synmin_note(hza,hzz,level,durms);
        } break;
    }
  }
  synmin_update(buffer,framec);
}

/* Annoying clang glue.
 * clang inserts calls to memset and memcpy despite our having told it "nostdlib".
 * Whatever, we can implement them.
 */

#if USE_web
void *memset(void *s, int n, long c) {
  unsigned char *p=s;
  for (;c-->0;p++) *p=n;
  return s;
}
#endif
