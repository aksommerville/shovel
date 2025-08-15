#include "genioc_internal.h"
#include <sys/time.h>
#include <time.h>

/* sh_log
 */

void sh_log(const char *msg) {
  fprintf(stderr,"GAME: %s\n",msg);
}

/* sh_ms
 * Lock audio driver and copy to the message queue.
 */

int sh_ms(const void *v,int c) {
  if (!v||(c<1)) return -1;
  if (genioc.msgc>=MSG_LIMIT) return -1;
  if (io_audio_lock()<0) return -1;
  int err=0;
  void *nv=malloc(c);
  if (!nv) {
    err=-1;
  } else {
    memcpy(nv,v,c);
    int p=genioc.msgp+genioc.msgc++;
    if (p>=MSG_LIMIT) p-=MSG_LIMIT;
    struct msg *msg=genioc.msgv+p;
    msg->v=nv;
    msg->c=c;
  }
  io_audio_unlock();
  return err;
}

/* sh_mr
 * Pop from the message queue.
 * Called from audio segment.
 */

int sh_mr(void *v,int a) {
  if (genioc.msgc<1) return 0;
  struct msg *msg=genioc.msgv+genioc.msgp++;
  if (genioc.msgp>=MSG_LIMIT) genioc.msgp=0;
  genioc.msgc--;
  if (msg->c<=a) memcpy(v,msg->v,msg->c);
  else memcpy(v,msg->v,a);
  int err=msg->c;
  if (msg->v) free(msg->v);
  msg->v=0;
  msg->c=0;
  return err;
}

/* sh_term
 */

void sh_term(int status) {
  genioc.sigc++;
  genioc.exitstatus=status;
}

/* sh_now
 */

double sh_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (double)tv.tv_sec+(double)tv.tv_usec/1000000.0;
}

// genioc private, not part of shovel api. but it belongs with its sister ^
double now_cpu() {
  struct timespec tv={0};
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&tv);
  return (double)tv.tv_sec+(double)tv.tv_nsec/1000000000.0;
}

/* sh_sg
 * Read from the store.
 */

int sh_sg(char *v,int va,const char *k,int kc) {
  if (!v||(va<0)) va=0;
  const struct field *field=genioc.fieldv;
  int i=genioc.fieldc;
  for (;i-->0;field++) {
    if (field->kc!=kc) continue;
    if (memcmp(field->k,k,kc)) continue;
    if (field->vc<=va) memcpy(v,field->v,field->vc);
    else memcpy(v,field->v,va);
    return field->vc;
  }
  return 0;
}

/* sh_ss
 * Write to the store and save it immediately if changed.
 */

int sh_ss(const char *k,int kc,const char *v,int vc) {
  if (!k||(kc<1)||(kc>0xffff)) return -1;
  if (!v||(vc<0)||(vc>0xffff)) return -1;
  struct field *field=genioc.fieldv;
  int i=0;
  for (;i<genioc.fieldc;i++,field++) {
    if (field->kc!=kc) continue;
    if (memcmp(field->k,k,kc)) continue;
    if (vc) {
      if ((vc==field->vc)&&!memcmp(field->v,v,vc)) return 0; // Unchanged.
      char *nv=malloc(vc+1);
      if (!nv) return -1;
      memcpy(nv,v,vc);
      nv[vc]=0;
      if (field->v) free(field->v);
      field->v=nv;
      field->vc=vc;
    } else {
      if (field->k) free(field->k);
      if (field->v) free(field->v);
      genioc.fieldc--;
      memmove(field,field+1,sizeof(struct field)*(genioc.fieldc-i));
    }
    return genioc_store_save();
  }
  if (!vc) return 0;
  if (genioc.fieldc>=genioc.fielda) {
    int na=genioc.fielda+16;
    if (na>INT_MAX/sizeof(struct field)) return -1;
    void *nv=realloc(genioc.fieldv,sizeof(struct field)*na);
    if (!nv) return -1;
    genioc.fieldv=nv;
    genioc.fielda=na;
  }
  char *nk=malloc(kc+1);
  if (!nk) return -1;
  char *nv=malloc(vc+1);
  if (!nv) { free(nk); return -1; }
  memcpy(nk,k,kc);
  nk[kc]=0;
  memcpy(nv,v,vc);
  nv[vc]=0;
  field=genioc.fieldv+genioc.fieldc++;
  field->k=nk;
  field->kc=kc;
  field->v=nv;
  field->vc=vc;
  return genioc_store_save();
}
