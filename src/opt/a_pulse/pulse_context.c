#include "pulse_internal.h"

const char *io_audio_driver_name="pulse";

struct pulse pulse={0};

/* Call out to the game to generate a signal, then interleave and quantize into (pulse.buf).
 */
 
static void pulse_interleave_and_quantize(int16_t *dst,int framec) {

  // It's common enough for games to produce mono but drivers to require stereo.
  // In that case, duplicate the left channel instead of just zeroing right.
  if ((pulse.chanc==2)&&pulse.ubuf[0]&&!pulse.ubuf[1]) {
    const float *srcp=pulse.ubuf[0];
    for (;framec-->0;dst+=2,srcp++) {
      dst[0]=dst[1]=(int16_t)((*srcp)*32000.0f);
    }
    return;
  }

  // Any other setup, it's generic. Copy the channels we have and zero the ones we don't.
  int chid=0; for (;chid<pulse.chanc;chid++) {
    int16_t *dstp=dst+chid;
    const float *srcp=(chid<pulse.ubufc)?pulse.ubuf[chid]:0;
    int i=framec;
    if (srcp) {
      for (;i-->0;dstp+=pulse.chanc,srcp++) {
        *dstp=(int16_t)((*srcp)*32000.0f);
      }
    } else {
      for (;i-->0;dstp+=pulse.chanc) *dstp=0;
    }
  }
}
 
static void pulse_solicit_signal() {
  int16_t *dst=pulse.buf;
  int updframec=pulse.bufa/pulse.chanc;
  while (updframec>0) {
    int upd1=updframec;
    if (upd1>pulse.ubuflen) upd1=pulse.ubuflen;
    sha_update(upd1);
    pulse_interleave_and_quantize(dst,upd1);
    dst+=upd1*pulse.chanc;
    updframec-=upd1;
  }
}

/* I/O thread.
 */
 
static void *pulse_iothd(void *arg) {
  while (1) {
    pthread_testcancel();
    
    if (pthread_mutex_lock(&pulse.iomtx)) {
      usleep(1000);
      continue;
    }
    if (pulse.running) {
      pulse_solicit_signal();
    } else {
      memset(pulse.buf,0,pulse.bufa<<1);
    }
    pthread_mutex_unlock(&pulse.iomtx);
    
    int err=0,result;
    pthread_testcancel();
    int pvcancel;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&pvcancel);
    result=pa_simple_write(pulse.pa,pulse.buf,sizeof(int16_t)*pulse.bufa,&err);
    pthread_setcancelstate(pvcancel,0);
    if (result<0) {
      pulse.ioerror=-1;
      return 0;
    }
  }
}

/* Delete.
 */

void io_audio_quit() {
  if (!pulse.rate) return;
  
  if (pulse.iothd) {
    pthread_cancel(pulse.iothd);
    pthread_join(pulse.iothd,0);
  }
  
  if (pulse.pa) pa_simple_free(pulse.pa);
  
  if (pulse.buf) free(pulse.buf);
  
  memset(&pulse,0,sizeof(pulse));
}

/* Init PulseAudio client.
 */
 
static int pulse_init_pa(const struct io_audio_setup *setup) {
  int err;
  
  const char *appname="Pulse Client";
  const char *servername=0;
  int buffersize=0;
  if (setup) {
    if (setup->rate>0) pulse.rate=setup->rate;
    if (setup->chanc>0) pulse.chanc=setup->chanc;
    if (setup->buffer>0) buffersize=setup->buffer;
  }
  if (pulse.rate<1) pulse.rate=44100;
  if (pulse.chanc<1) pulse.chanc=1;
  if (buffersize<1) buffersize=pulse.rate/100;
  if (buffersize<20) buffersize=20;

  pa_sample_spec sample_spec={
    #if BYTE_ORDER==BIG_ENDIAN
      .format=PA_SAMPLE_S16BE,
    #else
      .format=PA_SAMPLE_S16LE,
    #endif
    .rate=pulse.rate,
    .channels=pulse.chanc,
  };
  pa_buffer_attr buffer_attr={
    .maxlength=pulse.chanc*sizeof(int16_t)*buffersize,
    .tlength=pulse.chanc*sizeof(int16_t)*buffersize,
    .prebuf=0xffffffff,
    .minreq=0xffffffff,
  };
  
  if (!(pulse.pa=pa_simple_new(
    servername,
    appname,
    PA_STREAM_PLAYBACK,
    0, // sink name (?)
    appname,
    &sample_spec,
    0, // channel map
    &buffer_attr,
    &err
  ))) {
    return -1;
  }
  
  pulse.rate=sample_spec.rate;
  pulse.chanc=sample_spec.channels;
  
  return 0;
}

/* With the final rate and channel count settled, calculate a good buffer size and allocate it.
 */
 
static int pulse_init_buffer(const struct io_audio_setup *setup) {

  const double buflen_target_s= 0.010; // about 100 Hz
  const int buflen_min=           128; // but in no case smaller than N samples
  const int buflen_max=         16384; // ...nor larger
  
  // Initial guess and clamp to the hard boundaries.
  if (setup->buffer>0) pulse.bufa=setup->buffer;
  else pulse.bufa=buflen_target_s*pulse.rate*pulse.chanc;
  if (pulse.bufa<buflen_min) {
    pulse.bufa=buflen_min;
  } else if (pulse.bufa>buflen_max) {
    pulse.bufa=buflen_max;
  }
  // Reduce to next multiple of channel count.
  pulse.bufa-=pulse.bufa%pulse.chanc;
  
  if (!(pulse.buf=malloc(sizeof(int16_t)*pulse.bufa))) {
    return -1;
  }
  
  return 0;
}

/* Prepare mutex and thread.
 */
 
static int pulse_init_thread() {
  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr,PTHREAD_MUTEX_RECURSIVE);
  if (pthread_mutex_init(&pulse.iomtx,&mattr)) return -1;
  pthread_mutexattr_destroy(&mattr);
  if (pthread_create(&pulse.iothd,0,pulse_iothd,0)) return -1;
  return 0;
}

/* New.
 */

int io_audio_init(struct io_audio_setup *setup) {
  if (pulse.rate) return -1;
  pulse.ubuflen=INT_MAX;
  if (
    (pulse_init_pa(setup)<0)||
    (pulse_init_buffer(setup)<0)||
    (pulse_init_thread()<0)||
    (sha_init(pulse.rate,pulse.chanc)<0)
  ) return -1;
  return 0;
}

/* Prepare buffer.
 */

void sh_spcm(int chid,const float *pcm,int framec) {
  if (!pulse.rate) return;
  if ((chid<0)||(chid>=UBUF_LIMIT)) return;
  if (framec<1) return;
  pulse.ubuf[chid]=(float*)pcm;
  if (framec<pulse.ubuflen) pulse.ubuflen=framec;
  if (chid>=pulse.ubufc) pulse.ubufc=chid+1;
}

/* Trivial accessors.
 */

int io_audio_get_running() {
  return pulse.running;
}

int io_audio_set_running(int running) {
  if (!pulse.rate) return -1;
  pulse.running=running?1:0;
  return 0;
}

/* Update.
 */

int io_audio_update() {
  if (!pulse.rate) return 0;
  if (pulse.ioerror) return -1;
  return 0;
}

/* Lock.
 */
 
int io_audio_lock() {
  if (!pulse.rate) return -1;
  if (pthread_mutex_lock(&pulse.iomtx)) return -1;
  return 0;
}

void io_audio_unlock() {
  if (!pulse.rate) return;
  pthread_mutex_unlock(&pulse.iomtx);
}
