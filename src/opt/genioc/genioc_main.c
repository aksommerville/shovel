//TODO
#include <stdio.h>
#include "shovel/shovel.h"

static int terminate=0;

void shovel_log(const char *msg) {
  fprintf(stderr,"shovel_log: %s\n",msg);
}

void shovel_terminate(int status) {
  terminate=1;
}

int main(int argc,char **argv) {
  fprintf(stderr,"genioc main (XXX highly stub)\n");
  if (shm_init()<0) {
    fprintf(stderr,"shm_init failed\n");
    return 1;
  }
  if (sha_init(44100,2)<0) {
    fprintf(stderr,"sha_init failed\n");//TODO this needn't be fatal
    return 1;
  }
  fprintf(stderr,"genioc initialized game, running...\n");
  int panic=20;
  while (!terminate) {
    if (--panic<=0) {
      fprintf(stderr,"genioc terminating due to artificial limit\n");
      break;
    }
    shm_update(0.100);
    sha_update(4410);
  }
  shm_quit(0);
  fprintf(stderr,"genioc: Normal exit\n");
  return 0;
}
