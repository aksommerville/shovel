#include "evdev.h"
#include "shovel/shovel.h"
#include "opt/io/io.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Not including the aggregate player zero.
#define PLAYER_LIMIT 8

const char *io_input_driver_name="evdev";

static struct {
  struct evdev *evdev;
  uint8_t playerv[1+PLAYER_LIMIT];
} i_evdev={0};

/* Quit.
 */
 
void io_input_quit() {
  evdev_del(i_evdev.evdev);
  memset(&i_evdev,0,sizeof(i_evdev));
}

/* Callbacks.
 */
 
static void cb_connect(struct evdev *evdev,struct evdev_device *device) {
  int vid=0,pid=0,version=0;
  const char *name=evdev_device_get_ids(&vid,&pid,&version,device);
  fprintf(stderr,"%s %04x:%04x:%04x '%s'\n",__func__,vid,pid,version,name);
}
 
static void cb_disconnect(struct evdev *evdev,struct evdev_device *device) {
  fprintf(stderr,"%s\n",__func__);
}
 
static void cb_button(struct evdev *evdev,struct evdev_device *device,int type,int code,int value) {
  //TODO This is a ridiculous hack, hard-coding a few devices I've got handy. Replace with general input mapping.
  if ((type&~0xff)||(code&~0xffff)) return;
  int vid=0,pid=0,version=0;
  evdev_device_get_ids(&vid,&pid,&version,device);
  int plrid=0,btnid=0;
  char axis=0;
  // Do nothing to ignore it, or set (axis) to 'x' or 'y' and value to -1,0,1, or set (plrid,btnid).
  switch (vid) {
    case 0x45e: switch (pid) {
        case 0x289: switch ((type<<16)|code) { // Original Xbox.
            case 0x030010: plrid=1; axis='x'; break;
            case 0x030011: plrid=1; axis='y'; break;
            case 0x010130: plrid=1; btnid=SH_BTN_SOUTH; break;
            case 0x010133: plrid=1; btnid=SH_BTN_WEST; break;
            case 0x01013b: plrid=1; btnid=SH_BTN_AUX1; break;
          } break;
        case 0x028e: switch (version) {
            case 0x0105: switch ((type<<16)|code) { // Evercade. And probably real Xbox 360s too, but not my model.
                case 0x030010: plrid=1; axis='x'; break;
                case 0x030011: plrid=1; axis='y'; break;
                case 0x010130: plrid=1; btnid=SH_BTN_SOUTH; break;
                case 0x010133: plrid=1; btnid=SH_BTN_WEST; break;
                case 0x01013b: plrid=1; btnid=SH_BTN_AUX1; break;
              } break;
          } break;
      } break;
    case 0x0e8f: switch (pid) {
        case 0x0003: switch ((type<<16)|code) { // El Cheapo.
            case 0x030010: plrid=1; axis='x'; break;
            case 0x030011: plrid=1; axis='y'; break;
            case 0x010122: plrid=1; btnid=SH_BTN_SOUTH; break;
            case 0x010123: plrid=1; btnid=SH_BTN_WEST; break;
            case 0x010129: plrid=1; btnid=SH_BTN_AUX1; break;
          } break;
      } break;
  }
  //fprintf(stderr,"%s: %04x:%04x:%04x (0x%06x=%d) => plrid=%d btnid=0x%02x axis=%c\n",__func__,vid,pid,version,(type<<16)|code,value,plrid,btnid,axis);
  if (axis=='x') {
    if (value<0) {
      if (i_evdev.playerv[plrid]&SH_BTN_LEFT) return;
      i_evdev.playerv[plrid]|=SH_BTN_LEFT;
      i_evdev.playerv[0]|=SH_BTN_LEFT;
      i_evdev.playerv[plrid]&=~SH_BTN_RIGHT;
      i_evdev.playerv[0]&=~SH_BTN_RIGHT;
    } else if (value>0) {
      if (i_evdev.playerv[plrid]&SH_BTN_RIGHT) return;
      i_evdev.playerv[plrid]|=SH_BTN_RIGHT;
      i_evdev.playerv[0]|=SH_BTN_RIGHT;
      i_evdev.playerv[plrid]&=~SH_BTN_LEFT;
      i_evdev.playerv[0]&=~SH_BTN_LEFT;
    } else {
      if (!(i_evdev.playerv[plrid]&(SH_BTN_LEFT|SH_BTN_RIGHT))) return;
      i_evdev.playerv[plrid]&=~SH_BTN_LEFT;
      i_evdev.playerv[0]&=~SH_BTN_LEFT;
      i_evdev.playerv[plrid]&=~SH_BTN_RIGHT;
      i_evdev.playerv[0]&=~SH_BTN_RIGHT;
    }
  } else if (axis=='y') {
    if (value<0) {
      if (i_evdev.playerv[plrid]&SH_BTN_UP) return;
      i_evdev.playerv[plrid]|=SH_BTN_UP;
      i_evdev.playerv[0]|=SH_BTN_UP;
      i_evdev.playerv[plrid]&=~SH_BTN_DOWN;
      i_evdev.playerv[0]&=~SH_BTN_DOWN;
    } else if (value>0) {
      if (i_evdev.playerv[plrid]&SH_BTN_DOWN) return;
      i_evdev.playerv[plrid]|=SH_BTN_DOWN;
      i_evdev.playerv[0]|=SH_BTN_DOWN;
      i_evdev.playerv[plrid]&=~SH_BTN_UP;
      i_evdev.playerv[0]&=~SH_BTN_UP;
    } else {
      if (!(i_evdev.playerv[plrid]&(SH_BTN_UP|SH_BTN_DOWN))) return;
      i_evdev.playerv[plrid]&=~SH_BTN_UP;
      i_evdev.playerv[0]&=~SH_BTN_UP;
      i_evdev.playerv[plrid]&=~SH_BTN_DOWN;
      i_evdev.playerv[0]&=~SH_BTN_DOWN;
    }
  } else if (btnid) {
    if ((plrid<0)||(plrid>PLAYER_LIMIT)) return;
    if (value) {
      if (i_evdev.playerv[plrid]&btnid) return;
      i_evdev.playerv[plrid]|=btnid;
      i_evdev.playerv[0]|=btnid;
    } else {
      if (!(i_evdev.playerv[plrid]&btnid)) return;
      i_evdev.playerv[plrid]&=~btnid;
      i_evdev.playerv[0]&=~btnid;
    }
  }
}

/* Init.
 */

int io_input_init() {
  if (i_evdev.evdev) return -1;
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
 
//TODO Implement generic input mapping out where we can share it around.
static struct kmap { int keycode,plrid,btnid,value; } kmapv[]={
  {0x00070004,2,SH_BTN_LEFT},    // a
  {0x00070007,2,SH_BTN_RIGHT},   // d
  {0x00070008,2,SH_BTN_WEST},    // e
  {0x00070015,2,SH_BTN_AUX1},    // r
  {0x00070016,2,SH_BTN_DOWN},    // s
  {0x0007001a,2,SH_BTN_UP},      // w
  {0x00070028,1,SH_BTN_AUX1},    // enter
  {0x0007002c,2,SH_BTN_SOUTH},   // space
  {0x00070036,1,SH_BTN_SOUTH},   // comma
  {0x00070037,1,SH_BTN_WEST},    // dot
  {0x0007004f,1,SH_BTN_RIGHT},   // right
  {0x00070050,1,SH_BTN_LEFT},    // left
  {0x00070051,1,SH_BTN_DOWN},    // down
  {0x00070052,1,SH_BTN_UP},      // up
  {0x00070057,3,SH_BTN_WEST},    // kp+
  {0x00070058,3,SH_BTN_AUX1},    // kpenter
  {0x0007005a,3,SH_BTN_DOWN},    // kp2
  {0x0007005c,3,SH_BTN_LEFT},    // kp4
  {0x0007005d,3,SH_BTN_DOWN},    // kp5
  {0x0007005e,3,SH_BTN_RIGHT},   // kp6
  {0x00070060,3,SH_BTN_UP},      // kp8
  {0x00070062,3,SH_BTN_SOUTH},   // kp0
};

void io_input_set_key(int keycode,int value) {
  //fprintf(stderr,"%s 0x%08x=%d\n",__func__,keycode,value);
  struct kmap *kmap=0;
  int lo=0,hi=sizeof(kmapv)/sizeof(kmapv[0]);
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    struct kmap *q=kmapv+ck;
         if (keycode<q->keycode) hi=ck;
    else if (keycode>q->keycode) lo=ck+1;
    else { kmap=q; break; }
  }
  if (!kmap) return;
  if (value) {
    if (kmap->value) return;
    //TODO stateless actions
    kmap->value=1;
    i_evdev.playerv[kmap->plrid]|=kmap->btnid;
    i_evdev.playerv[0]|=kmap->btnid;
  } else {
    if (!kmap->value) return;
    kmap->value=0;
    i_evdev.playerv[kmap->plrid]&=~kmap->btnid;
    i_evdev.playerv[0]&=~kmap->btnid;
  }
}

/* Report a state to the game.
 */

int sh_in(int plrid) {
  if ((plrid<0)||(plrid>PLAYER_LIMIT)) return 0;
  return i_evdev.playerv[plrid];
}
