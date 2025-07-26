#include "xegl_internal.h"

/* Key press, release, or repeat.
 */
 
static int xegl_evt_key(struct xegl *xegl,XKeyEvent *evt,int value) {

  /* Pass the raw keystroke. */
  if (xegl->delegate.cb_key) {
    KeySym keysym=XkbKeycodeToKeysym(xegl->dpy,evt->keycode,0,0);
    if (keysym) {
      int keycode=xegl_usb_usage_from_keysym((int)keysym);
      if (keycode) {
        int err=xegl->delegate.cb_key(xegl->delegate.userdata,keycode,value);
        if (err) return err; // Stop here if acknowledged.
      }
    }
  }
  
  /* Pass text if press or repeat, and text can be acquired. */
  if (xegl->delegate.cb_text) {
    int shift=(evt->state&ShiftMask)?1:0;
    KeySym tkeysym=XkbKeycodeToKeysym(xegl->dpy,evt->keycode,0,shift);
    if (shift&&!tkeysym) { // If pressing shift makes this key "not a key anymore", fuck that and pretend shift is off
      tkeysym=XkbKeycodeToKeysym(xegl->dpy,evt->keycode,0,0);
    }
    if (tkeysym) {
      int codepoint=xegl_codepoint_from_keysym(tkeysym);
      if (codepoint && (evt->type == KeyPress || evt->type == KeyRepeat)) {
        xegl->delegate.cb_text(xegl->delegate.userdata,codepoint);
      }
    }
  }
  
  return 0;
}

/* Client message.
 */
 
static int xegl_evt_client(struct xegl *xegl,XClientMessageEvent *evt) {
  if (evt->message_type==xegl->atom_WM_PROTOCOLS) {
    if (evt->format==32) {
      if (evt->data.l[0]==xegl->atom_WM_DELETE_WINDOW) {
        if (xegl->delegate.cb_close) {
          xegl->delegate.cb_close(xegl->delegate.userdata);
        }
      }
    }
  }
  return 0;
}

/* Configuration event (eg resize).
 */
 
static int xegl_evt_configure(struct xegl *xegl,XConfigureEvent *evt) {
  int nw=evt->width,nh=evt->height;
  if ((nw!=xegl->w)||(nh!=xegl->h)) {
    xegl->w=nw;
    xegl->h=nh;
    if (xegl->delegate.cb_resize) {
      xegl->delegate.cb_resize(xegl->delegate.userdata,nw,nh);
    }
  }
  return 0;
}

/* Focus.
 */
 
static int xegl_evt_focus(struct xegl *xegl,XFocusInEvent *evt,int value) {
  if (value==xegl->focus) return 0;
  xegl->focus=value;
  if (xegl->delegate.cb_focus) {
    xegl->delegate.cb_focus(xegl->delegate.userdata,value);
  }
  return 0;
}

/* Process one event.
 */
 
static int xegl_receive_event(struct xegl *xegl,XEvent *evt) {
  if (!evt) return -1;
  switch (evt->type) {
  
    case KeyPress: return xegl_evt_key(xegl,&evt->xkey,1);
    case KeyRelease: return xegl_evt_key(xegl,&evt->xkey,0);
    case KeyRepeat: return xegl_evt_key(xegl,&evt->xkey,2);
    
    case ClientMessage: return xegl_evt_client(xegl,&evt->xclient);
    
    case ConfigureNotify: return xegl_evt_configure(xegl,&evt->xconfigure);
    
    case FocusIn: return xegl_evt_focus(xegl,&evt->xfocus,1);
    case FocusOut: return xegl_evt_focus(xegl,&evt->xfocus,0);
    
  }
  return 0;
}

/* Update.
 */
 
int xegl_update(struct xegl *xegl) {
  int evtc=XEventsQueued(xegl->dpy,QueuedAfterFlush);
  while (evtc-->0) {
    XEvent evt={0};
    XNextEvent(xegl->dpy,&evt);
    if ((evtc>0)&&(evt.type==KeyRelease)) {
      XEvent next={0};
      XNextEvent(xegl->dpy,&next);
      evtc--;
      if ((next.type==KeyPress)&&(evt.xkey.keycode==next.xkey.keycode)&&(evt.xkey.time>=next.xkey.time-XEGL_KEY_REPEAT_INTERVAL)) {
        evt.type=KeyRepeat;
        if (xegl_receive_event(xegl,&evt)<0) return -1;
      } else {
        if (xegl_receive_event(xegl,&evt)<0) return -1;
        if (xegl_receive_event(xegl,&next)<0) return -1;
      }
    } else {
      if (xegl_receive_event(xegl,&evt)<0) return -1;
    }
  }
  return 0;
}
