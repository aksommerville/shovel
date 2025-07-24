#include "genioc_internal.h"
#include <signal.h>
#include <unistd.h>

struct genioc genioc={0};

/* Signal handler.
 */
 
static void rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(genioc.sigc)>=3) {
        fprintf(stderr,"Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

/* Quit.
 */
 
static void genioc_quit() {
  double endtime_real=sh_now();
  double endtime_cpu=now_cpu();
  io_audio_quit();
  io_video_quit();
  io_input_quit();
  if (genioc.exitstatus) {
    fprintf(stderr,"%s: Abnormal exit.\n",genioc.exename);
  } else if (genioc.framec>0) {
    double elapsed_real=endtime_real-genioc.starttime_real;
    double elapsed_cpu=endtime_cpu-genioc.starttime_cpu;
    double avgrate=(double)genioc.framec/elapsed_real;
    double cpuload=elapsed_cpu/elapsed_real;
    fprintf(stderr,"%s: %d frames in %.03f s, avg %.03f Hz, cpu %.06f\n",genioc.exename,genioc.framec,elapsed_real,avgrate,cpuload);
  } else {
    fprintf(stderr,"%s: Normal exit.\n",genioc.exename);
  }
}

/* Video callbacks.
 */
 
static void genioc_cb_close() {
  genioc.sigc++;
}

static void genioc_cb_focus(int focus) {
  if (focus) {
    if (genioc.blurpause) {
      genioc.blurpause=0;
      io_audio_set_running(1);
    }
  } else {
    if (!genioc.blurpause) {
      genioc.blurpause=1;
      io_audio_set_running(0);
    }
  }
}

/* Init.
 */
 
static int genioc_init(int argc,char **argv) {
  int err;
  if ((err=genioc_store_load())<0) return err;
  {
    struct io_audio_setup setup={
      .rate=44100,//TODO
      .chanc=2,
      .device=0,
      .buffer=0,
    };
    if ((err=io_audio_init(&setup))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing audio (%s).\n",genioc.exename,io_audio_driver_name);
      return -2;
    }
  }
  {
    struct io_video_setup setup={
      .w=0,
      .h=0,
      .fbw=160,//TODO is this knowable?
      .fbh=90,
      .fullscreen=0,
      .title="TODO: Game title",
      .iconrgba=0,
      .iconw=0,
      .iconh=0,
      .cb_close=genioc_cb_close,
      .cb_focus=genioc_cb_focus,
    };
    if ((err=io_video_init(&setup))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing video (%s).\n",genioc.exename,io_video_driver_name);
      return -2;
    }
  }
  {
    if ((err=io_input_init())<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing input (%s).\n",genioc.exename,io_input_driver_name);
      return -2;
    }
  }
  fprintf(stderr,
    "%s: Initialized drivers: video=%s audio=%s input=%s\n",
    genioc.exename,io_video_driver_name,io_audio_driver_name,io_input_driver_name
  );
  if (shm_init()<0) {
    fprintf(stderr,"%s: Failed to initialize game.\n",genioc.exename);
    return -2;
  }
  io_audio_set_running(1);
  genioc.starttime_real=sh_now();
  genioc.starttime_cpu=now_cpu();
  genioc.recent_time=genioc.starttime_real-TARGET_PERIOD-0.001;
  return 0;
}

/* Update, blocking.
 */
 
static int genioc_update() {
  int err;

  // Tick the clock and force to a reasonable interval.
  double now=sh_now();
  double elapsed=now-genioc.recent_time;
  if (elapsed>0.500) { // Unreasonably high. Fault.
    elapsed=TARGET_PERIOD;
    genioc.clockfaultc++;
  } else if (elapsed<0.0) { // Impossibly low. Fault.
    elapsed=TARGET_PERIOD;
    genioc.clockfaultc++;
  } else while (elapsed<TARGET_PERIOD) {
    usleep((int)((TARGET_PERIOD-elapsed)*1000000.0)+500); // +500us because it's better to overshoot consistently than to need to repeat.
    now=sh_now();
    elapsed=now-genioc.recent_time;
  }
  if (elapsed<MIN_PERIOD) elapsed=MIN_PERIOD;
  else if (elapsed>MAX_PERIOD) elapsed=MAX_PERIOD;
  genioc.recent_time=now;
  genioc.framec++;

  // Update I/O drivers.
  if ((err=io_video_update())<0) {
    if (err!=-2) fprintf(stderr,"%s: Error updating video driver.\n",genioc.exename);
    return -2;
  }
  if (genioc.sigc) return 0; // Typically gets set during video update (ie close window).
  if ((err=io_audio_update())<0) {
    if (err!=-2) fprintf(stderr,"%s: Error updating audio driver.\n",genioc.exename);
    return -2;
  }
  if ((err=io_input_update())<0) {
    if (err!=-2) fprintf(stderr,"%s: Error updating input driver.\n",genioc.exename);
    return -2;
  }
  
  // If we're hard-paused, get out.
  if (genioc.blurpause) return 0;
  
  // Update game.
  shm_update(elapsed);
  
  // Commit the video frame if there is one (io figures it out).
  if ((err=io_video_commit())<0) {
    if (err!=-2) fprintf(stderr,"%s: Error sending video frame.\n",genioc.exename);
    return -2;
  }
  return 0;
}

/* Load store.
 */
 
int genioc_store_load() {
  fprintf(stderr,"TODO %s\n",__func__);
  return 0;
}

/* Save store.
 */
 
int genioc_store_save() {
  fprintf(stderr,"TODO %s\n",__func__);
  return 0;
}

/* Main.
 */

int main(int argc,char **argv) {
  signal(SIGINT,rcvsig);
  genioc.exename="shovel";
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) genioc.exename=argv[0];
  int err=genioc_init(argc,argv);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing.\n",genioc.exename);
    return 1;
  }
  while (!genioc.sigc) {
    if ((err=genioc_update())<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error updating.\n",genioc.exename);
      genioc.exitstatus=1;
      break;
    }
  }
  genioc_quit();
  return genioc.exitstatus;
}
