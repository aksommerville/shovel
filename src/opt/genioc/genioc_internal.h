#ifndef GENIOC_INTERNAL_H
#define GENIOC_INTERNAL_H

#include "genioc.h"
#include "shovel/shovel.h"
#include "opt/io/io.h"
#include "opt/fs/fs.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MSG_LIMIT 32
#define TARGET_PERIOD (1.0/60.0)
#define MIN_PERIOD 0.012
#define MAX_PERIOD 0.019

extern struct genioc {
  const char *exename;
  volatile int sigc;
  int exitstatus;
  double recent_time;
  int framec,clockfaultc;
  double starttime_real,starttime_cpu;
  int blurpause; // Nonzero if blurred.
  
  struct msg {
    void *v;
    int c;
  } msgv[MSG_LIMIT];
  int msgp,msgc;
  
  struct field {
    char *k,*v;
    int kc,vc;
  } *fieldv;
  int fieldc,fielda;
  
} genioc;

int genioc_store_load();
int genioc_store_save();

double now_cpu();

#endif
