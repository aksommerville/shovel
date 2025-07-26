/* audio/main.c
 *
 * Message format. Each message is identified by its leading byte.
 *   0x01 NOTE: [0x01,noteida 0..63,noteidz 0..63,level 0..31,duration 16ms]
 *   0x02 SONG: [0x02,songid]
 */

#include "shovel/shovel.h"
#include "opt/synmin/synmin.h"

extern const int meadowlark_len;
extern const unsigned char meadowlark[];

extern const int red_alarm_len;
extern const unsigned char red_alarm[];

extern const int we_need_norris_len;
extern const unsigned char we_need_norris[];

static const unsigned char song_abc[]={
#define DELAY(ms) (ms>>4)-1, // 16..2048
#define NOTE(a,z,level,dur16ms) 0x80|(a<<1)|(z>>5),0xff&((z<<3)|(level>>2)),0xff&((level<<6)|dur16ms), // note 0..63, level 0..31, dur 0..31
  NOTE(0x20,0x20,0x00,0x04)
  DELAY(96)
  NOTE(0x22,0x22,0x04,0x04)
  DELAY(96)
  NOTE(0x23,0x23,0x08,0x04)
  DELAY(96)
  NOTE(0x25,0x25,0x0c,0x04)
  DELAY(96)
  NOTE(0x27,0x27,0x10,0x04)
  DELAY(96)
  NOTE(0x28,0x28,0x14,0x04)
  DELAY(96)
  NOTE(0x2a,0x2a,0x18,0x04)
  DELAY(96)
  NOTE(0x2c,0x2c,0x1f,0x04)
  DELAY(96)
#undef DELAY
#undef NOTE
};

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
          synmin_note(msg[1],msg[2],msg[3],msg[4]);
        } break;
      case 0x02: if (msgc>=2) {
          const void *src=0;
          int srcc=0;
          switch (msg[1]) {
            case 0: break; // Explicitly empty.
            case 1: src=meadowlark; srcc=meadowlark_len; break;
            case 2: break; // Was even_tippier_toe, but that had too many unshovellable bits.
            case 3: src=song_abc; srcc=sizeof(song_abc); break;
            case 4: src=red_alarm; srcc=red_alarm_len; break;
            case 5: src=we_need_norris; srcc=we_need_norris_len; break;
          }
          synmin_song(src,srcc,0,1);
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
