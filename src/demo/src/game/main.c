#include "shovel.h"

unsigned char my_rom[256]={9,8,7,6,5,4,3,2,1};
int my_rom_size=sizeof(my_rom);
#define shovel_rom my_rom
#define shovel_rom_length my_rom_size

void shm_quit(int status) {
  shovel_log("shm_quit");
}

int shm_init() {
  shovel_log("shm_init");
  
  char msg[32];
  int msgc=0;
  msg[msgc++]='l';
  msg[msgc++]='e';
  msg[msgc++]='n';
  msg[msgc++]='=';
  if (shovel_rom_length>999) msg[msgc++]='>';
  if (shovel_rom_length>=100) msg[msgc++]='0'+(shovel_rom_length/100)%10;
  if (shovel_rom_length>=10) msg[msgc++]='0'+(shovel_rom_length/10)%10;
  msg[msgc++]='0'+shovel_rom_length%10;
  int srcp=0,srcc=shovel_rom_length;
  if (srcc>4) srcc=4;
  else if (!srcc) srcc=3;
  while (srcp<srcc) {
    msg[msgc++]=' ';
    msg[msgc++]="0123456789abcdef"[shovel_rom[srcp]>>4];
    msg[msgc++]="0123456789abcdef"[shovel_rom[srcp]&15];
    srcp++;
  }
  msg[msgc]=0;
  shovel_log(msg);
  
  return 0;
}

void shm_update(double elapsed) {
  shovel_log("shm_update; terminating");
  shovel_terminate(0);
}
