#ifndef GENIOC_INTERNAL_H
#define GENIOC_INTERNAL_H

#include "genioc.h"
#include "shovel/shovel.h"
#include "opt/io/io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MSG_LIMIT 32

extern struct genioc {
  const char *exename;
  volatile int sigc;
  int exitstatus;
  
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

#endif
