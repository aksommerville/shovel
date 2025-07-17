#include "v_dummy.h"
#include "shovel/shovel.h"
#include "opt/io/io.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>

const char *io_video_driver_name="dummy";
const int io_video_provides_events=0;

static struct {
  int init;
  const void *fb;
  int fbw,fbh;
} v_dummy={0};

/* Quit.
 */
 
void io_video_quit() {
  memset(&v_dummy,0,sizeof(v_dummy));
}

/* Init.
 */

int io_video_init(struct io_video_setup *setup) {
  if (v_dummy.init) return -1;
  // Real drivers would examine (setup) and prepare an output context here.
  v_dummy.init=1;
  return 0;
}

/* Poll for events.
 * Noop for us.
 */

int io_video_update() {
  return 0;
}

/* Receive framebuffer from game.
 */

void sh_fb(const void *rgbx,int w,int h) {
  v_dummy.fb=rgbx;
  v_dummy.fbw=w;
  v_dummy.fbh=h;
}

/* Commit frame.
 */
 
int io_video_commit() {
  if (!v_dummy.fb) return 0;
  //TODO Opportunity to capture video frames... We could trigger this via automation.
  v_dummy.fb=0;
  return 0;
}
