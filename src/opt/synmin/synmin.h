/* synmin.h
 * Bare-minimum synthesizer. For that "cinnamon" sound.
 * We have no dependencies, not even libc or libm.
 * We only use square waves, and no level envelopes.
 * Mono only.
 *
 * Incorporating in a Shovel game is dead simple, your audio segment is just this:
 *   #include "shovel/shovel.h"
 *   #include "opt/synmin/synmin.h"
 *   static float buffer[1024];
 *   int sha_init(int rate,int chanc) {
 *     if (synmin_init(rate,chanc)<0) return -1;
 *     sh_spcm(0,buffer,sizeof(buffer)/sizeof(buffer[0]));
 *     return 0;
 *   }
 *   void sha_update(int framec) {
 *     unsigned char msg[256];
 *     int msgc;
 *     while ((msgc=sh_mr(msg,sizeof(msg)))>0) {
 *       ...msg format is your problem. Turn into synmin calls...
 *     }
 *     synmin_update(buffer,framec);
 *   }
 *
 * Data format: TODO
 */
 
#ifndef SYNMIN_H
#define SYNMIN_H

int synmin_init(int rate,int chanc);
void synmin_update(float *v,int c);

void synmin_note(int hza,int hzz,float level,int durms);

#endif
