#include "synmin_internal.h"

struct synmin synmin={0};

/* Init.
 */
 
int synmin_init(int rate,int chanc) {
  if (synmin.rate) return -1;
  if ((rate<200)||(rate>200000)) return -1;
  if (chanc<1) return -1;
  synmin.rate=rate;
  
  /* Calculate the rate table.
   * To mitigate rounding errors, calculate the whole thing floating-point first.
   */
  float fpv[64];
  fpv[32]=(float)rate/440.0f; // Reference note: 32 = 440 hz
  const float TWELFTH_ROOT_TWO=1.0594630943592953f;
  int i=11;
  float *p=fpv+33;
  for (;i-->0;p++) *p=p[-1]/TWELFTH_ROOT_TWO;
  for (i=44,p=fpv+44;i<64;i++,p++) *p=p[-12]*0.5f;
  for (i=32,p=fpv+31;i-->0;p--) *p=p[12]*2.0f;
  int *dst=synmin.periodv;
  for (p=fpv,i=64;i-->0;dst++,p++) {
    *dst=(int)((*p)+0.5f);
    if (*dst<2) *dst=2;
  }
  
  return 0;
}

/* Replace song.
 */
 
void synmin_song(const void *v,int c,int force,int repeat) {
  if (c<1) v=0;
  if (!force&&(v==synmin.song)) {
    synmin.songrepeat=repeat;
    return;
  }
  synmin.song=v;
  synmin.songp=0;
  synmin.songc=c;
  synmin.songdelay=0;
  synmin.songrepeat=repeat;
  synmin.voicec=0;
}

/* Update voice.
 * Caller must check TTL and not call with any more than that.
 */
 
static void synmin_voice_update(float *v,int c,struct synmin_voice *voice) {
  int i=c;
  for (;i-->0;v++) {
    if (voice->p>=voice->fulltime) {
      voice->p=0;
      (*v)+=voice->level;
    } else if (voice->p>=voice->halftime) {
      voice->p++;
      (*v)-=voice->level;
    } else {
      voice->p++;
      (*v)+=voice->level;
    }
    if (--(voice->adjclock)<=0) {
      voice->adjclock+=voice->adjtime;
      voice->fulltime+=voice->adjd;
      voice->halftime=voice->fulltime>>1;
    }
  }
  voice->ttl-=c;
}

/* Generate signal.
 */
 
static void synmin_update_signal(float *v,int c) {
  struct synmin_voice *voice=synmin.voicev;
  int i=synmin.voicec;
  for (;i-->0;voice++) {
    int updc=voice->ttl;
    if (!updc) continue;
    if (updc>c) updc=c;
    synmin_voice_update(v,updc,voice);
  }
}

/* Frames from ms.
 */
 
static int synmin_frames_from_ms(int ms) {
  float durs=(float)ms/1000.0f;
  return (int)(durs*(float)synmin.rate);
}

/* Update song.
 * Call only when (songdelay==0).
 * Processes any note events and stops after populating songdelay or EOF.
 */
 
static void synmin_song_update() {
  int delayms=0;
  for (;;) {
    
    // EOF?
    if (synmin.songp>=synmin.songc) {
      if (synmin.songrepeat) {
        synmin.songp=0;
        if ((synmin.songdelay=synmin_frames_from_ms(delayms))<1) synmin.songdelay=1;
      } else {
        synmin.song=0;
      }
      return;
    }
    
    // Delay event? Consume and add it to the pile.
    if (!(synmin.song[synmin.songp]&0x80)) {
      delayms+=(synmin.song[synmin.songp++]+1)<<4;
      continue;
    }
    
    // If we've encountered any delays, convert to frames and we're done.
    if (delayms) {
      if ((synmin.songdelay=synmin_frames_from_ms(delayms))<1) synmin.songdelay=1;
      return;
    }
    
    // Note. 1aaaaaaz zzzzzlll lltttttt : NOTE (a)..(z), level (l), duration (t+1)*16 ms
    if (synmin.songp>synmin.songc-3) {
      synmin.song=0;
      return;
    }
    unsigned char a=synmin.song[synmin.songp++];
    unsigned char b=synmin.song[synmin.songp++];
    unsigned char c=synmin.song[synmin.songp++];
    unsigned char noteida=(a>>1)&63;
    unsigned char noteidz=((a<<5)&32)|(b>>3);
    unsigned char level=((b&7)<<2)|(c>>6);
    unsigned char dur16ms=c&63;
    synmin_note(noteida,noteidz,level,dur16ms);
  }
}

/* Update.
 */
 
void synmin_update(float *v,int c) {

  // Zero the buffer.
  {
    float *p=v;
    int i=c;
    for (;i-->0;p++) *p=0.0f;
  }
  
  // Let the song dictate update intervals.
  while (c>0) {
    int updc=c;
    if (synmin.song) {
      if (!synmin.songdelay) synmin_song_update();
      if ((synmin.songdelay>0)&&(synmin.songdelay<updc)) {
        updc=synmin.songdelay;
      }
      if ((synmin.songdelay-=updc)<0) synmin.songdelay=0;
    }
    synmin_update_signal(v,updc);
    v+=updc;
    c-=updc;
  }
  
  // Drop defunct voices from the tail.
  while (synmin.voicec&&!synmin.voicev[synmin.voicec-1].ttl) synmin.voicec--;
}

/* Begin note.
 */

void synmin_note(unsigned char noteida,unsigned char noteidz,unsigned char level,unsigned char dur16ms) {
  if (!synmin.rate) return;
  noteida&=0x3f;
  noteidz&=0x3f;
  level&=0x1f;
  dur16ms&=0x3f;

  /* If we're not using all available voices, just append to the list.
   * Otherwise, take the first we find that's not in use.
   * If none, overwrite the voice with the shortest TTL.
   */
  struct synmin_voice *voice=0;
  if (synmin.voicec<SYNMIN_VOICE_LIMIT) {
    voice=synmin.voicev+synmin.voicec++;
  } else {
    struct synmin_voice *q=synmin.voicev;
    voice=q;
    int i=synmin.voicec;
    for (;i-->0;q++) {
      if (!q->ttl) {
        voice=q;
        break;
      } else if (q->ttl<voice->ttl) {
        voice=q;
      }
    }
  }
  
  voice->p=0;
  if ((voice->ttl=synmin_frames_from_ms((1+dur16ms)<<4))<1) voice->ttl=1;
  voice->level=0.050f+level/100.0f;
  voice->fulltime=synmin.periodv[noteida];
  voice->halftime=voice->fulltime>>1;
  if (noteida==noteidz) {
    voice->adjtime=0;
    voice->adjclock=0;
    voice->adjd=0;
  } else {
    int ftz=synmin.periodv[noteidz];
    int dtotal=ftz-voice->fulltime;
    if ((dtotal>-voice->ttl)&&(dtotal<voice->ttl)) { // Typical. Sliding period by an amount less than the frame count.
      voice->adjtime=voice->ttl/dtotal;
      if (!voice->adjtime) voice->adjtime=1;
      else if (voice->adjtime<0) voice->adjtime=-voice->adjtime;
      if (!(voice->adjd=dtotal/voice->adjtime)) voice->adjd=(dtotal<0)?-1:1;
    } else { // Sliding by more than the frame count. Adjust on each frame.
      voice->adjtime=1;
      voice->adjd=dtotal/voice->ttl;
      if (!voice->adjd) voice->adjd=(dtotal<0)?-1:1;
    }
    voice->adjclock=voice->adjtime;
  }
}
