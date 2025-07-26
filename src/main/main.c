#include "shovel/shovel.h"
#include "opt/r1b/r1b.h"
#include <stdint.h>

R1B_DECL(witchy)
R1B_DECL(font6x8) // built-in spacing
R1B_DECL(font4x6) // built-in spacing
R1B_DECL(font3x3) // no spacing

#define FBW 64
#define FBH 36
static uint32_t fb[FBW*FBH];
static struct r1b_img32 fb_image={
  .v=fb,
  .w=FBW,
  .h=FBH,
  .stridewords=FBW,
};

static struct r1b_img1 witchy_image;
static struct r1b_img1 font6x8_image;
static struct r1b_img1 font4x6_image;
static struct r1b_img1 font3x3_image;

#define PLAYER_COUNT 3
static struct player {
  int pvin;
  int x,y;
  uint32_t color;
  int hilite;
  const struct r1b_img1 *image;
  int srcx,srcy,w,h;
  uint8_t xform;
} playerv[PLAYER_COUNT]={0};

static int songid=0;

/* Quit.
 */
 
void shm_quit(int status) {
  sh_log("shm_quit");
}

/* Init.
 */

int shm_init() {
  sh_log("shm_init");
  
  R1B_ASSIGN(witchy)
  R1B_ASSIGN(font6x8)
  R1B_ASSIGN(font4x6)
  R1B_ASSIGN(font3x3)
  
  playerv[0].x=1;
  playerv[0].y=FBH>>1;
  playerv[0].color=0xff0000ff;
  playerv[0].image=&witchy_image;
  playerv[0].srcx=8;
  playerv[0].srcy=0;
  playerv[0].w=8;
  playerv[0].h=8;
  playerv[0].xform=0;
  
  playerv[1].x=FBW>>1;
  playerv[1].y=FBH>>1;
  playerv[1].color=0xff00c000;
  playerv[1].image=&witchy_image;
  playerv[1].srcx=16;
  playerv[1].srcy=0;
  playerv[1].w=8;
  playerv[1].h=8;
  playerv[1].xform=0;
  
  playerv[2].x=FBW-2;
  playerv[2].y=FBH>>1;
  playerv[2].color=0xffff8000;
  playerv[2].image=&witchy_image;
  playerv[2].srcx=24;
  playerv[2].srcy=0;
  playerv[2].w=8;
  playerv[2].h=8;
  playerv[2].xform=R1B_XFORM_XREV;
  
  return 0;
}

/* Text.
 */
 
static void draw_string(int x,int y,const struct r1b_img1 *img,const char *src,int srcc,uint32_t xbgr,int extra) {
  int colw=img->w>>4;
  int rowh=img->h/6;
  int xstride=colw+extra;
  if (!src) return;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  for (;srcc-->0;src++,x+=xstride) {
    unsigned char ch=*src;
    if ((ch<=0x20)||(ch>=0x7f)) continue;
    ch-=0x20;
    int srcx=(ch&15)*colw;
    int srcy=(ch>>4)*rowh;
    r1b_img32_blit_img1(
      &fb_image,img,
      x,y,srcx,srcy,colw,rowh,
      0,xbgr,0
    );
  }
}

/* Update.
 */

void shm_update(double elapsed) {
  
  int i=0;
  struct player *player=playerv;
  for (;i<PLAYER_COUNT;i++,player++) {
    int in=sh_in(i+1);
    if (in!=player->pvin) {
    
      if (!i) { // first player also triggers synth events
        if ((in&SH_BTN_UP)&&!(player->pvin&SH_BTN_UP))       sh_ms("\x01\x20\x20\x10\x02",5);
        if ((in&SH_BTN_DOWN)&&!(player->pvin&SH_BTN_DOWN))   sh_ms("\x01\x14\x14\x10\x02",5);
        if ((in&SH_BTN_LEFT)&&!(player->pvin&SH_BTN_LEFT))   sh_ms("\x01\x18\x14\x10\x02",5);
        if ((in&SH_BTN_RIGHT)&&!(player->pvin&SH_BTN_RIGHT)) {
          //sh_ms("\x01\x18\x20\x10\x02",5);
          if (++songid>=6) songid=0;
          unsigned char msg[]={0x02,songid};
          sh_ms(msg,sizeof(msg));
        }
      }
    
      if ((in&SH_BTN_LEFT)&&!(player->pvin&SH_BTN_LEFT)&&(player->x>0)) player->x--;
      if ((in&SH_BTN_RIGHT)&&!(player->pvin&SH_BTN_RIGHT)&&(player->x<FBW-1)) player->x++;
      if ((in&SH_BTN_UP)&&!(player->pvin&SH_BTN_UP)&&(player->y>0)) player->y--;
      if ((in&SH_BTN_DOWN)&&!(player->pvin&SH_BTN_DOWN)&&(player->y<FBH-1)) player->y++;
      if ((in&SH_BTN_AUX1)&&!(player->pvin&SH_BTN_AUX1)) {
        sh_log("AUX1, terminating");
        sh_term(0);
      }
      if (in&SH_BTN_WEST) player->hilite=2;
      else if (in&SH_BTN_SOUTH) player->hilite=1;
      else player->hilite=0;
      player->pvin=in;
      if (0) {
        char msg[]={
          '0'+i+1,
          ':',
          ' ',
          (in&SH_BTN_LEFT)?'l':'.',
          (in&SH_BTN_RIGHT)?'r':'.',
          (in&SH_BTN_UP)?'u':'.',
          (in&SH_BTN_DOWN)?'d':'.',
          (in&SH_BTN_SOUTH)?'a':'.',
          (in&SH_BTN_WEST)?'b':'.',
          (in&SH_BTN_AUX1)?'s':'.',
          0,
        };
        sh_log(msg);
      }
    }
  }
  
  // A bit heavy-handed, but let's redraw the framebuffer from scratch each frame.
  r1b_img32_fill_rect(&fb_image,1,1,FBW-2,FBH-2,0xff000000);
  r1b_img32_fill_rect(&fb_image,0,0,FBW,1,0xffffffff);
  r1b_img32_fill_rect(&fb_image,0,0,1,FBH,0xffffffff);
  r1b_img32_fill_rect(&fb_image,0,FBH-1,FBW,1,0xffffffff);
  r1b_img32_fill_rect(&fb_image,FBW-1,0,1,FBH,0xffffffff);
  for (i=PLAYER_COUNT,player=playerv;i-->0;player++) {
    uint32_t color;
    if (player->hilite==2) color=0xffffffff;
    else if (player->hilite==1) color=0xff808080;
    else color=player->color;
    r1b_img32_blit_img1(
      &fb_image,player->image,
      player->x-(player->w>>1),player->y-(player->h>>1),
      player->srcx,player->srcy,player->w,player->h,
      0,color,player->xform
    );
  }
  
  const struct r1b_img1 *img=&font3x3_image;
  int extra=1;
  int ystride=img->h/6+1;
  draw_string(2,2+ystride* 0,img,"the quick brown",-1,0xffffffff,extra);
  draw_string(2,2+ystride* 1,img,"fox jumps over ",-1,0xffc0c0ff,extra);
  draw_string(2,2+ystride* 2,img,"the lazy dog   ",-1,0xffc0ffc0,extra);
  draw_string(2,2+ystride* 3,img,"12345678 times!",-1,0xffc0c0c0,extra);
  draw_string(2,2+ystride* 4,img,"A 64x64 frame  ",-1,0xffc0c0c0,extra);
  draw_string(2,2+ystride* 5,img,"buffer does not",-1,0xffc0c0c0,extra);
  draw_string(2,2+ystride* 6,img,"allow much room",-1,0xffc0c0c0,extra);
  draw_string(2,2+ystride* 7,img,"for text. :(   ",-1,0xffc0c0c0,extra);
  draw_string(2,2+ystride* 8,img,"OK row.        ",-1,0xffc0c0c0,extra);
  draw_string(2,2+ystride* 9,img,"Last valid row.",-1,0xffc0c0c0,extra);
  
  sh_fb(fb, FBW, FBH);
}

/* Annoying clang glue.
 * clang inserts calls to memset and memcpy despite our having told it "nostdlib".
 * Whatever, we can implement them.
 */

#if USE_web
void *memset(void *s, int n, long c) {
  unsigned char *p=s;
  for (;c-->0;p++) *p=n;
  return s;
}
#endif
