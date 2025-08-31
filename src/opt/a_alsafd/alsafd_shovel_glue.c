#include "alsafd_internal.h"

const char *io_audio_driver_name="alsafd";

#define UBUF_LIMIT 8

/* Private globals.
 */
 
static struct a_alsafd {
  struct alsafd *alsafd;
  const float *ubufv[UBUF_LIMIT]; // User buffers. WEAK, sparse.
  int ubufc;
  int ubuflen; // Frames, length of the shortest extant ubuf.
} a_alsafd={0};

/* Delete.
 */

void io_audio_quit() {
  alsafd_del(a_alsafd.alsafd);
  memset(&a_alsafd,0,sizeof(a_alsafd));
}

/* Quantize and interleave our user buffers into the driver's output buffer.
 * Caller has just arranged to fill the user buffers.
 */
 
static void a_alsafd_interleave(int16_t *dst,int framec) {
  int chanc=a_alsafd.alsafd->chanc;
  
  // Mono=>Stereo, extremely likely.
  if ((a_alsafd.ubufc==1)&&a_alsafd.ubufv[0]&&(chanc==2)) {
    const float *src=a_alsafd.ubufv[0];
    for (;framec-->0;src++) {
      int sample=(int)((*src)*32767.0f);
      if (sample<-32768) sample=-32768;
      else if (sample>32767) sample=32767;
      *dst++=sample;
      *dst++=sample;
    }
    return;
  }
  
  // Mono=>Mono, certainly could happen.
  if ((a_alsafd.ubufc==1)&&a_alsafd.ubufv[0]&&(chanc==1)) {
    const float *src=a_alsafd.ubufv[0];
    for (;framec-->0;src++,dst++) {
      int sample=(int)((*src)*32767.0f);
      if (sample<-32768) sample=-32768;
      else if (sample>32767) sample=32767;
      *dst=sample;
    }
    return;
  }
  
  // Stereo=>Stereo, not as likely as it sounds. (I don't expect Shovel games to use stereo ever, tho technically nothing's stopping them).
  if ((a_alsafd.ubufc==2)&&a_alsafd.ubufv[0]&&a_alsafd.ubufv[1]&&(chanc==2)) {
    const float *src=a_alsafd.ubufv[0];
    int samplec=framec<<1;
    for (;samplec-->0;src++,dst++) {
      int sample=(int)((*src)*32767.0f);
      if (sample<-32768) sample=-32768;
      else if (sample>32767) sample=32767;
      *dst=sample;
    }
    return;
  }
  
  // Fallback: Lots of redundant null checking, but we tolerate any channel count.
  int srcp=0;
  for (;framec-->0;srcp++) {
    int ui=0;
    for (;ui<chanc;ui++,dst++) {
      if ((ui<a_alsafd.ubufc)&&a_alsafd.ubufv[ui]) {
        float src=a_alsafd.ubufv[ui][srcp];
        if (src<=-1.0f) *dst=-32768;
        else if (src>=1.0f) *dst=32767;
        else *dst=(int16_t)(src*32767.f);
      } else {
        *dst=0;
      }
    }
  }
}

/* PCM callback.
 */
 
static void a_alsafd_cb_pcm(int16_t *v,int c,void *userdata) {
  int framec=c/a_alsafd.alsafd->chanc;
  while (framec>0) {
    int updc=framec;
    if (updc>a_alsafd.ubuflen) updc=a_alsafd.ubuflen;
    sha_update(updc);
    a_alsafd_interleave(v,updc);
    v+=framec*a_alsafd.alsafd->chanc;
    framec-=updc;
  }
}

/* Init.
 */

int io_audio_init(struct io_audio_setup *setup) {
  struct alsafd_delegate delegate={
    .userdata=0,
    .pcm_out=a_alsafd_cb_pcm,
  };
  struct alsafd_setup asetup={
    .rate=setup->rate,
    .chanc=setup->chanc,
    .device=setup->device,
    .buffersize=setup->buffer,
  };
  if (!(a_alsafd.alsafd=alsafd_new(&delegate,&asetup))) return -1;
  if (sha_init(a_alsafd.alsafd->rate,a_alsafd.alsafd->chanc)<0) return -1;
  return 0;
}

/* Receive user buffer. Part of initialization.
 * (technically we allow it at any time).
 */
 
void sh_spcm(int chid,const float *pcm,int framec) {
  if ((chid<0)||(chid>=UBUF_LIMIT)) return;
  if (!pcm||(framec<1)) return;
  a_alsafd.ubufv[chid]=pcm;
  if (!a_alsafd.ubuflen) a_alsafd.ubuflen=framec;
  else if (framec<a_alsafd.ubuflen) a_alsafd.ubuflen=framec;
  if (chid>=a_alsafd.ubufc) a_alsafd.ubufc=chid+1;
}

/* Trivial pass-thru.
 */
 
int io_audio_get_running() {
  return a_alsafd.alsafd->running;
}

int io_audio_set_running(int play) {
  alsafd_set_running(a_alsafd.alsafd,play);
  return a_alsafd.alsafd->running;
}

int io_audio_update() {
  return alsafd_update(a_alsafd.alsafd);
}

int io_audio_lock() {
  return alsafd_lock(a_alsafd.alsafd);
}

void io_audio_unlock() {
  alsafd_unlock(a_alsafd.alsafd);
}
