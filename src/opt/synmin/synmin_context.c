#include "synmin_internal.h"

struct synmin synmin={0};

/* Init.
 */
 
int synmin_init(int rate,int chanc) {
  if (synmin.rate) return -1;
  if ((rate<200)||(rate>200000)) return -1;
  if (chanc<1) return -1;
  synmin.rate=rate;
  return 0;
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

/* Update.
 */
 
void synmin_update(float *v,int c) {

  // Zero the buffer.
  {
    float *p=v;
    int i=c;
    for (;i-->0;p++) *p=0.0f;
  }
  
  //TODO song

  // Update voices.
  struct synmin_voice *voice=synmin.voicev;
  int i=synmin.voicec;
  for (;i-->0;voice++) {
    int updc=voice->ttl;
    if (!updc) continue;
    if (updc>c) updc=c;
    synmin_voice_update(v,updc,voice);
  }
  
  // Drop defunct voices from the tail.
  while (synmin.voicec&&!synmin.voicev[synmin.voicec-1].ttl) synmin.voicec--;
}

/* Begin note.
 */
 
void synmin_note(int hza,int hzz,float level,int durms) {
  if (!synmin.rate) return;
  if (hza<1) hza=1;
  if (hzz<1) hzz=1;
  if (durms<1) durms=1;

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
  voice->ttl=(int)(((float)durms*(float)synmin.rate)/1000.0f);
  voice->level=level;
  if ((voice->fulltime=synmin.rate/hza)<2) voice->fulltime=2;
  voice->halftime=voice->fulltime>>1;
  if (hza==hzz) {
    voice->adjtime=0;
    voice->adjclock=0;
    voice->adjd=0;
  } else {
    int ftz=synmin.rate/hzz;
    if (ftz<2) ftz=2;
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
