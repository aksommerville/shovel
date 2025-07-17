#include "i_dummy.h"
#include "shovel/shovel.h"
#include "opt/io/io.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>

// Count of real players allowed, not counting the phony player zero.
#define PLAYER_LIMIT 8

const char *io_input_driver_name="dummy";

static struct {
  int init;
  uint8_t playerv[1+PLAYER_LIMIT];
  struct kmap {
    int keycode;
    int plrid;
    int btnid;
    int value;
  } *kmapv;
  int kmapc,kmapa;
} i_dummy={0};

/* Quit.
 */
 
void io_input_quit() {
  if (i_dummy.kmapv) free(i_dummy.kmapv);
  memset(&i_dummy,0,sizeof(i_dummy));
}

/* Map list primitives.
 */
 
static int i_dummy_kmapv_search(int keycode) {
  int lo=0,hi=i_dummy.kmapc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=i_dummy.kmapv[ck].keycode;
         if (keycode<q) hi=ck;
    else if (keycode>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static struct kmap *i_dummy_kmapv_insert(int p,int keycode) {
  if ((p<0)||(p>i_dummy.kmapc)) return 0;
  if (i_dummy.kmapc>=i_dummy.kmapa) {
    int na=i_dummy.kmapa+16;
    if (na>INT_MAX/sizeof(struct kmap)) return 0;
    void *nv=realloc(i_dummy.kmapv,sizeof(struct kmap)*na);
    if (!nv) return 0;
    i_dummy.kmapv=nv;
    i_dummy.kmapa=na;
  }
  struct kmap *kmap=i_dummy.kmapv+p;
  memmove(kmap+1,kmap,sizeof(struct kmap)*(i_dummy.kmapc-p));
  i_dummy.kmapc++;
  memset(kmap,0,sizeof(struct kmap));
  kmap->keycode=keycode;
  return kmap;
}

/* Add mapping, convenience.
 */
 
static void i_dummy_kmap(int keycode,int plrid,int btnid) {
  if ((plrid<0)||(plrid>PLAYER_LIMIT)) return;
  int p=i_dummy_kmapv_search(keycode);
  if (p>=0) return;
  p=-p-1;
  struct kmap *kmap=i_dummy_kmapv_insert(p,keycode);
  if (!kmap) return;
  kmap->plrid=plrid;
  kmap->btnid=btnid;
}

/* Init.
 */

int io_input_init() {
  if (i_dummy.init) return -1;
  
  //TODO Read from Egg or Emuhost's config file. Implement that as a shareable unit, so all the input drivers can use it.
  i_dummy_kmap(0x00070050,1,SH_BTN_LEFT);    // left
  i_dummy_kmap(0x0007004f,1,SH_BTN_RIGHT);   // right
  i_dummy_kmap(0x00070052,1,SH_BTN_UP);      // up
  i_dummy_kmap(0x00070051,1,SH_BTN_DOWN);    // down
  i_dummy_kmap(0x00070036,1,SH_BTN_SOUTH);   // comma
  i_dummy_kmap(0x00070037,1,SH_BTN_WEST);    // dot
  i_dummy_kmap(0x00070028,1,SH_BTN_AUX1);    // enter
  i_dummy_kmap(0x00070004,2,SH_BTN_LEFT);    // a
  i_dummy_kmap(0x00070007,2,SH_BTN_RIGHT);   // d
  i_dummy_kmap(0x0007001a,2,SH_BTN_UP);      // w
  i_dummy_kmap(0x00070016,2,SH_BTN_DOWN);    // s
  i_dummy_kmap(0x0007002c,2,SH_BTN_SOUTH);   // space
  i_dummy_kmap(0x00070008,2,SH_BTN_WEST);    // e
  i_dummy_kmap(0x00070015,2,SH_BTN_AUX1);    // r
  i_dummy_kmap(0x0007005c,3,SH_BTN_LEFT);    // kp4
  i_dummy_kmap(0x0007005e,3,SH_BTN_RIGHT);   // kp6
  i_dummy_kmap(0x00070060,3,SH_BTN_UP);      // kp8
  i_dummy_kmap(0x0007005d,3,SH_BTN_DOWN);    // kp5
  i_dummy_kmap(0x0007005a,3,SH_BTN_DOWN);    // kp2
  i_dummy_kmap(0x00070062,3,SH_BTN_SOUTH);   // kp0
  i_dummy_kmap(0x00070057,3,SH_BTN_WEST);    // kp+
  i_dummy_kmap(0x00070058,3,SH_BTN_AUX1);    // kpenter
  
  i_dummy.init=1;
  return 0;
}

/* Update.
 */

int io_input_update() {
  //TODO Allow automation scripts.
  return 0;
}

/* Receive event from video driver.
 */

void io_input_set_key(int keycode,int value) {
  int p=i_dummy_kmapv_search(keycode);
  if (p<0) return;
  struct kmap *kmap=i_dummy.kmapv+p;
  //TODO stateless actions
  if (value) {
    if (kmap->value) return;
    kmap->value=1;
    i_dummy.playerv[kmap->plrid]|=kmap->btnid;
    i_dummy.playerv[0]|=kmap->btnid;
  } else {
    if (!kmap->value) return;
    kmap->value=0;
    i_dummy.playerv[kmap->plrid]&=~kmap->btnid;
    i_dummy.playerv[0]&=~kmap->btnid;
  }
}

/* Get state of a virtual device.
 */

int sh_in(int plrid) {
  if ((plrid<0)||(plrid>PLAYER_LIMIT)) return 0;
  return i_dummy.playerv[plrid];
}
