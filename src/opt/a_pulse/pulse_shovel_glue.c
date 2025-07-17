#include "pulse.h"
#include "shovel/shovel.h"
#include "opt/io/io.h"
#include <string.h>
#include <limits.h>
#include <stdlib.h>

#define CHANC_LIMIT 8

const char *io_audio_driver_name="pulse";

static struct {
  struct pulse *pulse;
  int rate,chanc;
  const float *pcmv[CHANC_LIMIT];
  int buflen;
  int ready; // Set after shm_init completes.
} a_pulse={0};

/* Quit.
 */

void io_audio_quit() {
  pulse_del(a_pulse.pulse);
  memset(&a_pulse,0,sizeof(a_pulse));
}

/* Convert the game's multichannel floats to the driver's interleaved ints.
 */
 
static void a_pulse_merge_and_quantize(int16_t *dst,int framec) {

  // It's common enough for games to produce mono but drivers to require stereo.
  // In that case, duplicate the left channel instead of just zeroing right.
  if ((a_pulse.chanc==2)&&a_pulse.pcmv[0]&&!a_pulse.pcmv[1]) {
    const float *srcp=a_pulse.pcmv[0];
    for (;framec-->0;dst+=2,srcp++) {
      dst[0]=dst[1]=(int16_t)((*srcp)*32000.0f);
    }
    return;
  }

  // Any other setup, it's generic. Copy the channels we have and zero the ones we don't.
  int chid=0; for (;chid<a_pulse.chanc;chid++) {
    int16_t *dstp=dst+chid;
    const float *srcp=a_pulse.pcmv[chid];
    int i=framec;
    if (srcp) {
      for (;i-->0;dstp+=a_pulse.chanc,srcp++) {
        *dstp=(int16_t)((*srcp)*32000.0f);
      }
    } else {
      for (;i-->0;dstp+=a_pulse.chanc) *dstp=0;
    }
  }
}

/* PCM callback.
 */
 
static void cb_pcm_out(int16_t *v,int samplec,void *userdata) {
  int framec=samplec/a_pulse.chanc;
  while (framec>0) {
    int updc=framec;
    if (updc>a_pulse.buflen) updc=a_pulse.buflen;
    sha_update(updc);
    a_pulse_merge_and_quantize(v,updc);
    framec-=updc;
    v+=updc*a_pulse.chanc;
  }
}

/* Init.
 */

int io_audio_init(struct io_audio_setup *setup) {
  if (a_pulse.pulse) return -1;
  struct pulse_delegate pdelegate={
    .pcm_out=cb_pcm_out,
  };
  struct pulse_setup psetup={
    .rate=setup->rate,
    .chanc=setup->chanc,
    .buffersize=setup->buffer,
  };
  if (!(a_pulse.pulse=pulse_new(&pdelegate,&psetup))) return -1;
  a_pulse.rate=pulse_get_rate(a_pulse.pulse);
  a_pulse.chanc=pulse_get_chanc(a_pulse.pulse);
  if ((a_pulse.chanc<1)||(a_pulse.chanc>CHANC_LIMIT)) return -1;
  a_pulse.buflen=INT_MAX;
  if (sha_init(a_pulse.rate,a_pulse.chanc)<0) return -1;
  a_pulse.ready=1;
  return 0;
}

/* Prepare buffer.
 */

void sh_spcm(int chid,const float *pcm,int framec) {
  if (!a_pulse.pulse||a_pulse.ready) return;
  if ((chid<0)||(chid>=CHANC_LIMIT)) return;
  if (framec<1) return;
  a_pulse.pcmv[chid]=pcm;
  if (framec<a_pulse.buflen) a_pulse.buflen=framec;
}

/* Trivial forwards to the non-shovel pulse driver.
 */

int io_audio_set_running(int running) {
  pulse_set_running(a_pulse.pulse,running);
  return 0;
}

int io_audio_get_running() {
  return pulse_get_running(a_pulse.pulse);
}

int io_audio_update() {
  return pulse_update(a_pulse.pulse);
}

int io_audio_lock() {
  return pulse_lock(a_pulse.pulse);
}

void io_audio_unlock() {
  pulse_unlock(a_pulse.pulse);
}
