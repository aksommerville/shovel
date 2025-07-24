#include "shovel/shovel.h"

static float buffer[1024];
static int halfperiod;
static int p=0;
static float level=0.125;

/* Init.
 */
 
int sha_init(int rate,int chanc) {
  sh_log("sha_init");
  halfperiod=rate/440;
  sh_spcm(0,buffer,sizeof(buffer)/sizeof(buffer[0]));
  return 0;
}

/* Update.
 */
 
void sha_update(int framec) {

  unsigned char msg;
  while (sh_mr(&msg,1)>=1) {
    switch (msg) {
      case 1: halfperiod<<=1; break;
      case 0xff: halfperiod>>=1; break;
    }
  }

  float *dst=buffer;
  for (;framec-->0;dst++) {
    *dst=level;
    p++;
    if (p>=halfperiod) {
      p=0;
      level=-level;
    }
  }
}
