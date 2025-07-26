#include "tool_internal.h"
#include "opt/midi/midi.h"

/* MIDI from MIDI.
 */
 
int tool_convert_midi(struct sr_encoder *dst,const void *src,int srcc,const char *path) {
  // One can imagine all kinds of normalization and validation.
  return sr_encode_raw(dst,src,srcc);
}

/* Synmin from MIDI.
 */
 
int tool_convert_synmin_midi(struct sr_encoder *dst,const void *src,int srcc,const char *path) {
  
  // "1000" causes the reader to operate in milliseconds.
  struct midi_file *file=midi_file_new(src,srcc,1000);
  if (!file) {
    if (!path) return -1;
    fprintf(stderr,"%s: Failed to decode MIDI file.\n",path);
    return -2;
  }
  
  #define HOLD_LIMIT 64
  struct smm_hold {
    int chid,noteid,starttime,durp; // dst[durp] is the low 6 bits, don't touch the top 2.
  } holdv[HOLD_LIMIT];
  int holdc=0;
  
  int delay=0;
  int nowms=0; // Includes pending delay.
  // Flushing may leave some residue, since we can only delay in 16ms quanta.
  #define FLUSH_DELAY { \
    while (delay>=2048) { \
      sr_encode_u8(dst,0x7f); \
      delay-=2048; \
    } \
    if (delay>=16) { \
      sr_encode_u8(dst,(delay>>4)-1); \
      delay&=15; \
    } \
  }
  for (;;) {
    struct midi_event event={0};
    int err=midi_file_next(&event,file);
    if (err<0) {
      if (midi_file_is_finished(file)) break;
      midi_file_del(file);
      if (!path) return -1;
      fprintf(stderr,"%s: Error streaming MIDI file.\n",path);
      return -2;
    }
    if (err) {
      if (midi_file_advance(file,err)<0) {
        midi_file_del(file);
        if (!path) return -1;
        fprintf(stderr,"%s: Error streaming MIDI file.\n",path);
        return -2;
      }
      nowms+=err;
      delay+=err;
      continue;
    }
    switch (event.opcode) {
      case 0x80: {
          struct smm_hold *hold=holdv;
          int i=0;
          for (;i<holdc;i++,hold++) {
            if (hold->chid!=event.chid) continue;
            if (hold->noteid!=event.a) continue;
            int durms=nowms-hold->starttime;
            int dur=(durms>>4)-1;
            if (dur<0) dur=0;
            else if (dur>0x3f) {
              dur=0x3f;
              if (path) fprintf(stderr,"%s:WARNING: Truncating long note 0x%02x from %d ms to 1024 ms.\n",path,event.a,durms);
            }
            if ((hold->durp>=0)&&(hold->durp<dst->c)) {
              ((uint8_t*)dst->v)[hold->durp]=(((uint8_t*)dst->v)[hold->durp]&0xc0)|dur;
            }
            holdc--;
            memmove(hold,hold+1,sizeof(struct smm_hold)*(holdc-i));
            break;
          }
        } break;
      case 0x90: {
          int noteid=event.a-37;
          if ((noteid<0)||(noteid>0x3f)) {
            if (path) fprintf(stderr,"%s:WARNING: Note ID 0x%02x not representable in synmin.\n",path,event.a);
            break;
          }
          int l=event.b>>2;
          FLUSH_DELAY
          sr_encode_u8(dst,0x80|(noteid<<1)|(noteid>>5));
          sr_encode_u8(dst,(noteid<<3)|(l>>2));
          sr_encode_u8(dst,(l<<6)); // 6 low bits reserved for duration
          if (holdc>=HOLD_LIMIT) {
            if (path) fprintf(stderr,"%s:WARNING: Hold limit %d exceeded. Some notes will have minimum length.\n",path,HOLD_LIMIT);
          } else {
            struct smm_hold *hold=holdv+holdc++;
            hold->chid=event.chid;
            hold->noteid=event.a;
            hold->starttime=nowms;
            hold->durp=dst->c-1;
          }
        } break;
    }
  }
  FLUSH_DELAY
  #undef FLUSH_DELAY
  #undef HOLD_LIMIT
  
  midi_file_del(file);
  return sr_encoder_require(dst,0);
}

/* C from MIDI.
 * In general, it's not MIDI that we want at runtime.
 * So this first effects an intermediate conversion, then turns that into C.
 */
 
int tool_convert_c_midi(struct sr_encoder *dst,const void *src,int srcc,const char *path,const char *dstpath) {

  const char *midfmt=0;
  int midfmtc=tool_arg_string(&midfmt,"synth",5);
  struct sr_encoder mid={0};
  if (!midfmtc) {
    sr_encode_raw(&mid,src,srcc);
  } else if ((midfmtc==4)&&!memcmp(midfmt,"midi",4)) {
    sr_encode_raw(&mid,src,srcc);
  } else if ((midfmtc==6)&&!memcmp(midfmt,"synmin",6)) {
    int err=tool_convert_synmin_midi(&mid,src,srcc,path);
    if (err<0) {
      sr_encoder_cleanup(&mid);
      return err;
    }
  } else {
    if (!path) return -1;
    fprintf(stderr,"%s: Unexpected format '--synth=%.*s'. Expected 'midi' or 'synmin'.\n",path,midfmtc,midfmt);
    return -2;
  }
  if (sr_encoder_require(&mid,0)<0) {
    sr_encoder_cleanup(&mid);
    return -1;
  }
  
  int err=tool_convert_c_any(dst,mid.v,mid.c,path,dstpath);
  sr_encoder_cleanup(&mid);
  return err;
}
