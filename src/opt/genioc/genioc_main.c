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
  //TODO Print performance stats if available.
  io_audio_quit();
  io_video_quit();
  io_input_quit();
  if (genioc.exitstatus) {
    fprintf(stderr,"%s: Abnormal exit.\n",genioc.exename);
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
  fprintf(stderr,"TODO: %s %d\n",__func__,focus); //TODO hard pause when blurred
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
  if (shm_init()<0) {
    fprintf(stderr,"%s: Failed to initialize game.\n",genioc.exename);
    return -2;
  }
  io_audio_set_running(1);
  //TODO Initialize performance monitor.
  return 0;
}

/* Update, blocking.
 */
 
static int genioc_update() {
  double elapsed=0.100;//TODO Timing.
  usleep((int)(elapsed*1000000.0));
  int err;
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
  shm_update(elapsed);
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
