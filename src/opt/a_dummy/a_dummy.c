#include "a_dummy.h"
#include "shovel/shovel.h"
#include "opt/io/io.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>

#define CHANC_LIMIT 8

const char *io_audio_driver_name="dummy";

static struct {
  int init;
  int rate;
  int chanc;
  int running;
  double last_update_time;
  const float *pcmv[CHANC_LIMIT];
  int buflen; // Size of the smallest of (pcmv), in frames.
} a_dummy={0};

/* Quit.
 */
 
void io_audio_quit() {
  memset(&a_dummy,0,sizeof(a_dummy));
}

/* Init.
 */

int io_audio_init(struct io_audio_setup *setup) {
  if (a_dummy.init) return -1;
  a_dummy.rate=setup->rate;
  if ((a_dummy.rate<2000)||(a_dummy.rate>200000)) a_dummy.rate=44100;
  a_dummy.chanc=setup->chanc;
  if ((a_dummy.chanc<1)||(a_dummy.chanc>CHANC_LIMIT)) a_dummy.chanc=2;
  a_dummy.buflen=INT_MAX;
  if (sha_init(a_dummy.rate,a_dummy.chanc)<0) return -1;
  a_dummy.init=1;
  return 0;
}

/* Trivial accessors.
 */
 
void sh_spcm(int chid,const float *pcm,int framec) {
  if (a_dummy.init) return; // This is only valid during shm_init(), at which time we haven't set (a_dummy.init) yet.
  if (framec<1) return;
  if ((chid<0)||(chid>=a_dummy.chanc)) return;
  a_dummy.pcmv[chid]=pcm;
  if (framec<a_dummy.buflen) a_dummy.buflen=framec;
}

int io_audio_set_running(int running) {
  if (a_dummy.running=running) {
    a_dummy.last_update_time=sh_now();
  }
  return 0;
}

int io_audio_get_running() {
  return a_dummy.running;
}

int io_audio_lock() {
  return 0;
}

void io_audio_unlock() {
}

/* Update single pass of signal shovel.
 */
 
static void a_dummy_update(int framec) {
  sha_update(framec);
  //TODO Fresh signal is present at (a_dummy.pcmv). We could record that to a file or something.
}

/* Update.
 * Unlike most real audio drivers, we'll operate synchronously.
 */

int io_audio_update() {
  if (!a_dummy.running) return 0;
  double now=sh_now();
  double elapseds=now-a_dummy.last_update_time;
  a_dummy.last_update_time=now;
  int elapsedframes=(int)(elapseds*(double)a_dummy.rate);
  while (elapsedframes>0) {
    int updc=elapsedframes;
    if (updc>a_dummy.buflen) updc=a_dummy.buflen;
    a_dummy_update(updc);
    elapsedframes-=updc;
  }
  return 0;
}
