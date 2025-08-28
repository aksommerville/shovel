#include "evdev_internal.h"
#include "shovel/shovel.h"
#include "opt/io/io.h"
#include "opt/inmgr/inmgr.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

const char *io_input_driver_name="evdev";

static struct {
  struct evdev *evdev;
  int devid_next;
} i_evdev={0};

/* Quit.
 */
 
void io_input_quit() {
  evdev_del(i_evdev.evdev);
  inmgr_quit();
  memset(&i_evdev,0,sizeof(i_evdev));
}

/* Callbacks.
 */
 
static int cb_cap(int type,int code,int hidusage,int lo,int hi,int value,void *userdata) {
  inmgr_connect_more(userdata,(type<<16)|code,hidusage,lo,hi,value);
  return 0;
}
 
static void cb_connect(struct evdev *evdev,struct evdev_device *device) {
  device->devid=i_evdev.devid_next++;
  int vid=0,pid=0,version=0;
  const char *name=evdev_device_get_ids(&vid,&pid,&version,device);
  void *ctx=inmgr_connect_begin(device->devid,vid,pid,version,name,-1);
  if (!ctx) return;
  evdev_device_for_each_button(device,cb_cap,ctx);
  if (inmgr_connect_end(ctx)>0) {
    fprintf(stderr,"Connected device %04x:%04x:%04x '%s'\n",vid,pid,version,name);
  } else {
    fprintf(stderr,"Ignoring device %04x:%04x:%04x '%s'\n",vid,pid,version,name);
  }
}
 
static void cb_disconnect(struct evdev *evdev,struct evdev_device *device) {
  inmgr_disconnect(device->devid);
}
 
static void cb_button(struct evdev *evdev,struct evdev_device *device,int type,int code,int value) {
  //fprintf(stderr,"%s %d.0x%08x=%d\n",__func__,device->devid,(type<<16)|code,value);
  inmgr_event(device->devid,(type<<16)|code,value);
}

/* Init.
 */

int io_input_init() {
  if (i_evdev.evdev) return -1;
  i_evdev.devid_next=1;
  inmgr_init();
  inmgr_set_buttons(INMGR_BTN_LEFT|INMGR_BTN_RIGHT|INMGR_BTN_UP|INMGR_BTN_DOWN|INMGR_BTN_SOUTH|INMGR_BTN_WEST|INMGR_BTN_AUX1);
  
  // Connect the system keyboard.
  if (io_video_provides_events) {
    void *ctx=inmgr_connect_begin(0,0,0,0,"System Keyboard",15);
    inmgr_connect_keyboard(ctx);
    inmgr_connect_end(ctx);
  }
  
  struct evdev_delegate delegate={
    .cb_connect=cb_connect,
    .cb_disconnect=cb_disconnect,
    .cb_button=cb_button,
  };
  if (!(i_evdev.evdev=evdev_new(0,&delegate))) return -1;
  return 0;
}

/* Update.
 */

int io_input_update() {
  return evdev_update(i_evdev.evdev);
}

/* Receive keyboard event.
 */

void io_input_set_key(int keycode,int value) {
  inmgr_event(0,keycode,value);
}

/* Report a state to the game.
 */

int sh_in(int plrid) {
  int state=inmgr_get_player(plrid);
  if (state) {
    state=
      ((state&INMGR_BTN_LEFT)?SH_BTN_LEFT:0)|
      ((state&INMGR_BTN_RIGHT)?SH_BTN_RIGHT:0)|
      ((state&INMGR_BTN_UP)?SH_BTN_UP:0)|
      ((state&INMGR_BTN_DOWN)?SH_BTN_DOWN:0)|
      ((state&INMGR_BTN_SOUTH)?SH_BTN_SOUTH:0)|
      ((state&INMGR_BTN_WEST)?SH_BTN_WEST:0)|
      ((state&INMGR_BTN_AUX1)?SH_BTN_AUX1:0)|
    0;
  }
  return state;
}
