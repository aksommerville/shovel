//XXX very temporary

#include <stdio.h>
#include <unistd.h>
#include "shovel.h"

static int terminate=0;
static int exitstatus=0;

const unsigned char shovel_rom[]={
  0x11,0x22,0x33,0x44,
};
const int shovel_rom_length=sizeof(shovel_rom);

void shovel_log(const char *msg) {
  fprintf(stderr,"shovel_log: %s\n",msg);
}

void shovel_terminate(int status) {
  fprintf(stderr,"shovel_terminate: %d\n",status);
  terminate=1;
  exitstatus=status;
}

int main(int argc,char **argv) {
  fprintf(stderr,"shovel native main\n");
  int err=shm_init();
  if (err<0) {
    fprintf(stderr,"error %d from shm_init()\n",err);
    return 1;
  }
  while (!terminate) {
    usleep(100000);
    shm_update(0.100);
  }
  shm_quit(exitstatus);
  return exitstatus;
}
