#include "inmgr_internal.h"

struct inmgr inmgr={0};

/* Quit.
 */
 
void inmgr_device_cleanup(struct inmgr_device *device) {
  if (device->buttonv) free(device->buttonv);
}
 
void inmgr_quit() {
  gcfg_input_del(inmgr.gcfg);
  if (inmgr.signalv) free(inmgr.signalv);
  if (inmgr.devicev) {
    while (inmgr.devicec-->0) inmgr_device_cleanup(inmgr.devicev+inmgr.devicec);
    free(inmgr.devicev);
  }
  if (inmgr.connect.unsetv) free(inmgr.connect.unsetv);
  memset(&inmgr,0,sizeof(struct inmgr));
}

/* Init.
 */
 
int inmgr_init() {
  if (inmgr.init) return -1;
  inmgr.init=1;
  inmgr.gcfg=gcfg_input_get();
  inmgr.playerc=1;
  inmgr.btnmask=0x7fff; // By default everything is enabled.
  return 0;
}

/* Set player count.
 */

void inmgr_set_player_count(int playerc) {
  if (playerc<1) playerc=1; else if (playerc>INMGR_PLAYER_LIMIT) playerc=INMGR_PLAYER_LIMIT;
  if (playerc==inmgr.playerc) return;
  
  /* Wipe global input state, clear any device player assignments that are now out of range, then rebuild state.
   * Do not create new assignments. The first device to speak up will get the newly vacant slots.
   */
  memset(inmgr.playerv,0,sizeof(inmgr.playerv));
  struct inmgr_device *device=inmgr.devicev;
  int i=inmgr.devicec;
  for (;i-->0;device++) {
    if (device->playerid>playerc) {
      device->playerid=0;
    } else if (inmgr.playerv[device->playerid]) {
      // There's more than one device assigned to this player. Keep that first one, but clear any others.
      // Imagine the app switches from 1-player to 4-player, and users have already interacted with all 4 devices.
      device->playerid=0;
    } else {
      // First device per still-valid playerid, keep it.
      inmgr.playerv[device->playerid]|=device->state;
      inmgr.playerv[0]|=device->state;
    }
  }

  inmgr.playerc=playerc;
}

/* Set button mask.
 */
 
void inmgr_set_buttons(int btnmask) {
  inmgr.btnmask=btnmask&0xffff;
}

/* Search signal handlers.
 */

int inmgr_signalv_search(int btnid) {
  int lo=0,hi=inmgr.signalc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=inmgr.signalv[ck].btnid;
         if (btnid<q) hi=ck;
    else if (btnid>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Register signal.
 */
 
void inmgr_set_signal(int btnid,void (*cb)()) {
  if (!(btnid&~0xffff)||!cb) return;
  int p=inmgr_signalv_search(btnid);
  if (p>=0) {
    inmgr.signalv[p].cb=cb;
    return;
  }
  p=-p-1;
  if (inmgr.signalc>=inmgr.signala) {
    int na=inmgr.signala+8;
    if (na>INT_MAX/sizeof(struct inmgr_signal)) return;
    void *nv=realloc(inmgr.signalv,sizeof(struct inmgr_signal)*na);
    if (!nv) return;
    inmgr.signalv=nv;
    inmgr.signala=na;
  }
  struct inmgr_signal *signal=inmgr.signalv+p;
  memmove(signal+1,signal,sizeof(struct inmgr_signal)*(inmgr.signalc-p));
  inmgr.signalc++;
  signal->btnid=btnid;
  signal->cb=cb;
}

/* Get player state.
 */

int inmgr_get_player(int playerid) {
  if (playerid<0) return 0;
  if (playerid>inmgr.playerc) return 0;
  return inmgr.playerv[playerid];
}

/* Search device list.
 */
 
int inmgr_devicev_search(int devid) {
  int lo=0,hi=inmgr.devicec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=inmgr.devicev[ck].devid;
         if (devid<q) hi=ck;
    else if (devid>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Add device.
 */
 
struct inmgr_device *inmgr_device_add(int devid) {
  int p=inmgr_devicev_search(devid);
  if (p>=0) return 0;
  p=-p-1;
  if (inmgr.devicec>=inmgr.devicea) {
    int na=inmgr.devicea+8;
    if (na>INT_MAX/sizeof(struct inmgr_device)) return 0;
    void *nv=realloc(inmgr.devicev,sizeof(struct inmgr_device)*na);
    if (!nv) return 0;
    inmgr.devicev=nv;
    inmgr.devicea=na;
  }
  struct inmgr_device *device=inmgr.devicev+p;
  memmove(device+1,device,sizeof(struct inmgr_device)*(inmgr.devicec-p));
  inmgr.devicec++;
  memset(device,0,sizeof(struct inmgr_device));
  device->devid=devid;
  return device;
}

/* Button list in device.
 */
 
static int inmgr_device_buttonv_search(const struct inmgr_device *device,int srcbtnid) {
  int lo=0,hi=device->buttonc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=device->buttonv[ck].srcbtnid;
         if (srcbtnid<q) hi=ck;
    else if (srcbtnid>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

struct inmgr_button *inmgr_device_add_button(struct inmgr_device *device,int srcbtnid) {
  int p=inmgr_device_buttonv_search(device,srcbtnid);
  if (p>=0) return 0;
  p=-p-1;
  if (device->buttonc>=device->buttona) {
    int na=device->buttona+16;
    if (na>INT_MAX/sizeof(struct inmgr_button)) return 0;
    void *nv=realloc(device->buttonv,sizeof(struct inmgr_button)*na);
    if (!nv) return 0;
    device->buttonv=nv;
    device->buttona=na;
  }
  struct inmgr_button *button=device->buttonv+p;
  memmove(button+1,button,sizeof(struct inmgr_button)*(device->buttonc-p));
  device->buttonc++;
  memset(button,0,sizeof(struct inmgr_button));
  button->srcbtnid=srcbtnid;
  return button;
}

struct inmgr_button *inmgr_device_get_button(const struct inmgr_device *device,int srcbtnid) {
  int p=inmgr_device_buttonv_search(device,srcbtnid);
  if (p<0) return 0;
  return device->buttonv+p;
}

/* Remove device.
 */
 
void inmgr_device_remove(struct inmgr_device *device) {
  int p=device-inmgr.devicev;
  if ((p<0)||(p>=inmgr.devicec)) return;
  inmgr_device_cleanup(device);
  inmgr.devicec--;
  memmove(device,device+1,sizeof(struct inmgr_device)*(inmgr.devicec-p));
}
