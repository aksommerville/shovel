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
 * Data format
 * No header, just a stream of events identifiable by the high bit of their leading byte:
 *   0ttttttt                   : DELAY (t+1)*16 ms
 *   1aaaaaaz zzzzzlll lltttttt : NOTE (a)..(z), level (l), duration (t+1)*16 ms
 */
 
#ifndef SYNMIN_H
#define SYNMIN_H

int synmin_init(int rate,int chanc);
void synmin_update(float *v,int c);

/* To play a note, you provide it as encoded:
 *   noteida,noteidz: 0..63
 *   level: 0..31; zero is not silent and 31 is not full amp
 *   dur16ms: 0..63; +1
 */
void synmin_note(unsigned char noteida,unsigned char noteidz,unsigned char level,unsigned char dur16ms);

/* Stop all voices and begin playing a song.
 * Caller must keep it in scope for as long as it's playing; one expects it comes from static data.
 */
void synmin_song(const void *v,int c,int force,int repeat);

#endif
