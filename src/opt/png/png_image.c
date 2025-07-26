#include "png.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Delete.
 */
 
static void png_chunk_cleanup(struct png_chunk *chunk) {
  if (chunk->v) free(chunk->v);
}
 
void png_image_del(struct png_image *image) {
  if (!image) return;
  if (image->v) free(image->v);
  if (image->chunkv) {
    while (image->chunkc-->0) png_chunk_cleanup(image->chunkv+image->chunkc);
    free(image->chunkv);
  }
  free(image);
}

/* New.
 */

struct png_image *png_image_new(int w,int h,int depth,int colortype) {
  if ((w<1)||(h<1)) return 0;
  int chanc;
  switch (colortype) {
    case 0: chanc=1; break;
    case 2: chanc=3; break;
    case 3: chanc=1; break;
    case 4: chanc=2; break;
    case 6: chanc=4; break;
    default: return 0;
  }
  int pixelsize=depth*chanc;
  if ((pixelsize<1)||(pixelsize>64)) return 0;
  if (pixelsize<8) {
    if (8%pixelsize) return 0;
  } else {
    if (pixelsize&7) return 0;
  }
  if (w>(INT_MAX-7)/pixelsize) return 0;
  int stride=(w*pixelsize+7)>>3;
  if (stride>INT_MAX/h) return 0;
  struct png_image *image=calloc(1,sizeof(struct png_image));
  if (!image) return 0;
  if (!(image->v=calloc(stride,h))) {
    free(image);
    return 0;
  }
  image->w=w;
  image->h=h;
  image->stride=stride;
  image->depth=depth;
  image->colortype=colortype;
  image->pixelsize=pixelsize;
  return image;
}

/* Add chunk.
 */

struct png_chunk *png_image_add_chunk(struct png_image *image,uint32_t id,const void *v,int c) {
  if (!image||(c<0)||(c&&!v)) return 0;
  if (image->chunkc>=image->chunka) {
    int na=image->chunka+8;
    if (na>INT_MAX/sizeof(struct png_chunk)) return 0;
    void *nv=realloc(image->chunkv,sizeof(struct png_chunk)*na);
    if (!nv) return 0;
    image->chunkv=nv;
    image->chunka=na;
  }
  void *nv=malloc(c?c:1);
  if (!nv) return 0;
  memcpy(nv,v,c);
  struct png_chunk *chunk=image->chunkv+image->chunkc++;
  chunk->v=nv;
  chunk->c=c;
  chunk->id=id;
  return chunk;
}

/* Remove chunk.
 */

void png_image_remove_chunk_at(struct png_image *image,int p) {
  if (!image||(p<0)||(p>=image->chunkc)) return;
  struct png_chunk *chunk=image->chunkv+p;
  png_chunk_cleanup(chunk);
  image->chunkc--;
  memmove(chunk,chunk+1,sizeof(struct png_chunk)*(image->chunkc-p));
}

/* Replace PLTE or tRNS chunk for an indexed image.
 * (src) is RGBA for tRNS, and for PLTE it may be RGB or RGBA.
 * Caller should trim tRNS count in advance.
 */
 
static int png_replace_PLTE(struct png_chunk *chunk,const uint8_t *src,int colorc,int chanc) {

  // Reallocate if necessary.
  int reqc=colorc*3;
  if (reqc>chunk->c) {
    void *nv=malloc(reqc);
    if (!nv) return -1;
    if (chunk->v) free(chunk->v);
    chunk->v=nv;
  }
  
  // Copy colors.
  if (chanc==3) {
    memcpy(chunk->v,src,reqc);
  } else if (chanc==4) {
    uint8_t *dst=chunk->v;
    int i=colorc;
    for (;i-->0;dst+=3,src+=4) {
      dst[0]=src[0];
      dst[1]=src[1];
      dst[2]=src[2];
    }
  } else {
    return -1;
  }
  chunk->c=reqc;
  
  return 0;
}

static int png_replace_tRNS(struct png_chunk *chunk,const uint8_t *src,int colorc) {
  
  // Reallocate if necessary.
  if (colorc>chunk->c) {
    void *nv=malloc(colorc);
    if (!nv) return -1;
    if (chunk->v) free(chunk->v);
    chunk->v=nv;
  }
  
  // Copy alphas.
  uint8_t *dst=chunk->v;
  const uint8_t *p=src+3;
  int i=0;
  for (;i<colorc;i++,p+=4,dst++) *dst=*p;
  chunk->c=colorc;
  
  return 0;
}

/* Set PLTE and tRNS.
 */
 
int png_image_set_ctab(struct png_image *image,const void *v,int c,int chanc) {
  if (!image||(c<0)||(c&&!v)||(chanc<3)||(chanc>4)) return -1;
  if (c>256) c=256;
  
  /* How large does tRNS need to be?
   * If they give us an RGBA color table but all the alphas are 0xff, don't store a tRNS.
   * We can trim 0xffs from the end.
   */
  int alphac=0;
  if (chanc==4) {
    alphac=c;
    while ((alphac>0)&&(((uint8_t*)v)[alphac*4-1]==0xff)) alphac--;
  }
  
  /* Examine existing chunks.
   * If (c==0), delete PLTE and tRNS.
   * If (chanc==3), delete tRNS.
   * If (chanc==4), replace tRNS.
   * Replace PLTE.
   */
  int gotplte=0,gottrns=0;
  int i=image->chunkc;
  while (i-->0) {
    struct png_chunk *chunk=image->chunkv+i;
    if (chunk->id==PNG_CHUNKID_PLTE) {
      if (!c) {
        png_chunk_cleanup(chunk);
        image->chunkc--;
        memmove(chunk,chunk+1,sizeof(struct png_chunk)*(image->chunkc-i));
      } else {
        if (png_replace_PLTE(chunk,v,c,chanc)<0) return -1;
        gotplte=1;
      }
    } else if (chunk->id==PNG_CHUNKID_tRNS) {
      if (!alphac) {
        png_chunk_cleanup(chunk);
        image->chunkc--;
        memmove(chunk,chunk+1,sizeof(struct png_chunk)*(image->chunkc-i));
      } else {
        if (png_replace_tRNS(chunk,v,c)<0) return -1;
        gottrns=1;
      }
    }
  }
  if (!c) return 0;
  
  // Add PLTE.
  if (!gotplte) {
    struct png_chunk *chunk=png_image_add_chunk(image,PNG_CHUNKID_PLTE,0,0);
    if (!chunk) return -1;
    if (png_replace_PLTE(chunk,v,c,chanc)<0) return -1;
  }
  
  // Add tRNS.
  if (!gottrns&&alphac) {
    struct png_chunk *chunk=png_image_add_chunk(image,PNG_CHUNKID_tRNS,0,0);
    if (!chunk) return -1;
    if (png_replace_tRNS(chunk,v,alphac)<0) return -1;
  }
  
  return 0;
}
