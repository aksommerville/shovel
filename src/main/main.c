#include "shovel/shovel.h"
#include <stdint.h>

#define FBW 64
#define FBH 64
#define PLAYER_COUNT 3
static uint32_t fb[FBW*FBH];
static struct player {
  int pvin;
  int x,y;
  uint32_t color;
  int hilite;
} playerv[PLAYER_COUNT]={0};

/* Quit.
 */
 
void shm_quit(int status) {
  shovel_log("shm_quit");
}

/* Init.
 */

int shm_init() {
  shovel_log("shm_init");
  
  playerv[0].x=1;
  playerv[0].y=FBH>>1;
  playerv[0].color=0xff0000ff;
  
  playerv[1].x=FBW>>1;
  playerv[1].y=FBH>>1;
  playerv[1].color=0xff00c000;
  
  playerv[2].x=FBW-2;
  playerv[2].y=FBH>>1;
  playerv[2].color=0xffff8000;
  
  return 0;
}

/* Rendering.
 */
 
static void fill_rect(int x,int y,int w,int h,uint32_t color) { // Must be in bounds.
  uint32_t *dstrow=fb+y*FBW+x;
  for (;h-->0;dstrow+=FBW) {
    uint32_t *dstp=dstrow;
    int xi=w;
    for (;xi-->0;dstp++) *dstp=color;
  }
}

/* Update.
 */

void shm_update(double elapsed) {
  
  int i=0;
  struct player *player=playerv;
  for (;i<PLAYER_COUNT;i++,player++) {
    int in=shovel_get_input(i+1);
    if (in!=player->pvin) {
      if ((in&SHOVEL_BTN_LEFT)&&!(player->pvin&SHOVEL_BTN_LEFT)&&(player->x>0)) player->x--;
      if ((in&SHOVEL_BTN_RIGHT)&&!(player->pvin&SHOVEL_BTN_RIGHT)&&(player->x<FBW-1)) player->x++;
      if ((in&SHOVEL_BTN_UP)&&!(player->pvin&SHOVEL_BTN_UP)&&(player->y>0)) player->y--;
      if ((in&SHOVEL_BTN_DOWN)&&!(player->pvin&SHOVEL_BTN_DOWN)&&(player->y<FBH-1)) player->y++;
      if ((in&SHOVEL_BTN_AUX1)&&!(player->pvin&SHOVEL_BTN_AUX1)) {
        shovel_log("AUX1, terminating");
        shovel_terminate(0);
      }
      if (in&SHOVEL_BTN_WEST) player->hilite=2;
      else if (in&SHOVEL_BTN_SOUTH) player->hilite=1;
      else player->hilite=0;
      player->pvin=in;
    }
  }
  
  // A bit heavy-handed, but let's redraw the framebuffer from scratch each frame.
  fill_rect(1,1,FBW-2,FBH-2,0xff000000);
  fill_rect(0,0,FBW,1,0xffffffff);
  fill_rect(0,0,1,FBH,0xffffffff);
  fill_rect(0,FBH-1,FBW,1,0xffffffff);
  fill_rect(FBW-1,0,1,FBH,0xffffffff);
  for (i=PLAYER_COUNT,player=playerv;i-->0;player++) {
    uint32_t color;
    if (player->hilite==2) color=0xffffffff;
    else if (player->hilite==1) color=0xff808080;
    else color=player->color;
    fb[player->y*FBW+player->x]=color;
  }
  
  shovel_set_framebuffer(fb, FBW, FBH);
}

/* Annoying clang glue.
 * clang inserts calls to memset and memcpy despite our having told it "nostdlib".
 * Whatever, we can implement them.
 */

#if USE_web
void *memset(void *s, int c, long n) {
  unsigned char *p=s;
  for (;c-->0;p++) *p=n;
  return s;
}
#endif
