#include "png.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Step iterator.
 */
 
static int png_next_1(struct png_iterator *iter) {
  if (iter->xc) {
    iter->xc--;
    if ((iter->shift-=1)<0) {
      iter->shift=7;
      iter->p++;
    }
    return 1;
  }
  if (iter->yc) {
    iter->yc--;
    iter->shift=7;
    iter->rowstart+=iter->image->stride;
    iter->p=iter->rowstart;
    iter->xc=iter->image->w-1;
    return 1;
  }
  return 0;
}
 
static int png_next_2(struct png_iterator *iter) {
  if (iter->xc) {
    iter->xc--;
    if ((iter->shift-=2)<0) {
      iter->shift=6;
      iter->p++;
    }
    return 1;
  }
  if (iter->yc) {
    iter->yc--;
    iter->shift=6;
    iter->rowstart+=iter->image->stride;
    iter->p=iter->rowstart;
    iter->xc=iter->image->w-1;
    return 1;
  }
  return 0;
}
 
static int png_next_4(struct png_iterator *iter) {
  if (iter->xc) {
    iter->xc--;
    if (iter->shift) {
      iter->shift=0;
    } else {
      iter->shift=4;
      iter->p++;
    }
    return 1;
  }
  if (iter->yc) {
    iter->yc--;
    iter->shift=4;
    iter->rowstart+=iter->image->stride;
    iter->p=iter->rowstart;
    iter->xc=iter->image->w-1;
    return 1;
  }
  return 0;
}

static int png_next_bytewise(struct png_iterator *iter) {
  if (iter->xc) {
    iter->xc--;
    iter->p+=iter->image->pixelsize>>3;
    return 1;
  }
  if (iter->yc) {
    iter->yc--;
    iter->rowstart+=iter->image->stride;
    iter->p=iter->rowstart;
    iter->xc=iter->image->w-1;
    return 1;
  }
  return 0;
}

/* Pixel accessors: Luma
 */
 
static struct png_pixel png_read_y1(struct png_iterator *iter) {
  int luma=((*(iter->p))&(1<<iter->shift))?0xffff:0x0000;
  return (struct png_pixel){
    .r=luma,
    .g=luma,
    .b=luma,
    .a=0xffff,
  };
}

static void png_write_y1(struct png_iterator *iter,struct png_pixel pixel) {
  int luma3=pixel.r+pixel.g+pixel.b;
  if (luma3>=0x18000) {
    (*(iter->p))|=1<<iter->shift;
  } else {
    (*(iter->p))&=~(1<<iter->shift);
  }
}
 
static struct png_pixel png_read_y2(struct png_iterator *iter) {
  int luma=((*(iter->p))>>iter->shift)&0x03;
  luma|=luma<<2;
  luma|=luma<<4;
  luma|=luma<<8;
  return (struct png_pixel){
    .r=luma,
    .g=luma,
    .b=luma,
    .a=0xffff,
  };
}

static void png_write_y2(struct png_iterator *iter,struct png_pixel pixel) {
  int luma=((pixel.r+pixel.g+pixel.b)/3)>>14;
  uint8_t mask=3<<iter->shift;
  *(iter->p)=((*(iter->p))&~mask)|(luma<<iter->shift);
}
 
static struct png_pixel png_read_y4(struct png_iterator *iter) {
  int luma=((*(iter->p))>>iter->shift)&0x0f;
  luma|=luma<<4;
  luma|=luma<<8;
  return (struct png_pixel){
    .r=luma,
    .g=luma,
    .b=luma,
    .a=0xffff,
  };
}

static void png_write_y4(struct png_iterator *iter,struct png_pixel pixel) {
  int luma=((pixel.r+pixel.g+pixel.b)/3)>>12;
  if (iter->shift) {
    *(iter->p)=((*(iter->p))&0x0f)|(luma<<4);
  } else {
    *(iter->p)=((*(iter->p))&0xf0)|luma;
  }
}
 
static struct png_pixel png_read_y8(struct png_iterator *iter) {
  int luma=iter->p[0];
  luma|=luma<<8;
  return (struct png_pixel){
    .r=luma,
    .g=luma,
    .b=luma,
    .a=0xffff,
  };
}

static void png_write_y8(struct png_iterator *iter,struct png_pixel pixel) {
  iter->p[0]=((pixel.r+pixel.g+pixel.b)/3)>>8;
}
 
static struct png_pixel png_read_y16(struct png_iterator *iter) {
  int luma=(iter->p[0]<<8)|iter->p[1];
  return (struct png_pixel){
    .r=luma,
    .g=luma,
    .b=luma,
    .a=0xffff,
  };
}

static void png_write_y16(struct png_iterator *iter,struct png_pixel pixel) {
  int luma=(pixel.r+pixel.g+pixel.b)/3;
  iter->p[0]=luma>>8;
  iter->p[1]=luma;
}

/* Pixel accessors: RGB
 */
 
static struct png_pixel png_read_rgb8(struct png_iterator *iter) {
  return (struct png_pixel){
    .r=(iter->p[0]<<8)|iter->p[0],
    .g=(iter->p[1]<<8)|iter->p[1],
    .b=(iter->p[2]<<8)|iter->p[2],
    .a=0xffff,
  };
}

static void png_write_rgb8(struct png_iterator *iter,struct png_pixel pixel) {
  iter->p[0]=pixel.r>>8;
  iter->p[1]=pixel.g>>8;
  iter->p[2]=pixel.b>>8;
}
 
static struct png_pixel png_read_rgb16(struct png_iterator *iter) {
  return (struct png_pixel){
    .r=(iter->p[0]<<8)|iter->p[1],
    .g=(iter->p[2]<<8)|iter->p[3],
    .b=(iter->p[4]<<8)|iter->p[5],
    .a=0xffff,
  };
}

static void png_write_rgb16(struct png_iterator *iter,struct png_pixel pixel) {
  iter->p[0]=pixel.r>>8;
  iter->p[1]=pixel.r;
  iter->p[2]=pixel.g>>8;
  iter->p[3]=pixel.g;
  iter->p[4]=pixel.b>>8;
  iter->p[5]=pixel.b;
}

/* Pixel accessors: Indexed
 */
 
static struct png_pixel png_ctlookup(struct png_iterator *iter,int ix) {
  // Scan the entire chunk list for every pixel. Ouch.
  struct png_pixel pixel={.a=0xffff};
  const struct png_chunk *chunk=iter->image->chunkv;
  int i=iter->image->chunkc;
  for (;i-->0;chunk++) {
    if (chunk->id==PNG_CHUNKID_PLTE) {
      int pltec=chunk->c/3;
      if (ix<pltec) {
        uint8_t *src=chunk->v;
        src+=ix*3;
        pixel.r=(src[0]<<8)|src[0];
        pixel.g=(src[1]<<8)|src[1];
        pixel.b=(src[2]<<8)|src[2];
      }
    } else if (chunk->id==PNG_CHUNKID_tRNS) {
      if (ix<chunk->c) {
        pixel.a=*(uint8_t*)chunk->v;
        pixel.a|=pixel.a<<8;
      }
    }
  }
  return pixel;
}

static int png_ctsearch(struct png_iterator *iter,struct png_pixel pixel) {
  // If you thought ctlookup was ugly, get a load of this:
  const uint8_t *plte=0,*trns=0;
  int pltec=0,trnsc=0; // pltec is entries, not bytes
  const struct png_chunk *chunk=iter->image->chunkv;
  int i=iter->image->chunkc;
  for (;i-->0;chunk++) {
    if (chunk->id==PNG_CHUNKID_PLTE) {
      plte=chunk->v;
      pltec=chunk->c/3;
    } else if (chunk->id==PNG_CHUNKID_tRNS) {
      trns=chunk->v;
      trnsc=chunk->c;
    }
  }
  if (!pltec) {
    int luma=(pixel.r+pixel.g+pixel.b)/3;
    return luma>>(16-iter->image->pixelsize);
  }
  pixel.r>>8; // We are only interested in the MSB.
  pixel.g>>8;
  pixel.b>>8;
  pixel.a>>8;
  int bestix=0,bestscore=1024;
  int ix=0;
  for (;ix<pltec;ix++,plte+=3) {
    int dr=plte[0]-pixel.r; if (dr<0) dr=-dr;
    int dg=plte[1]-pixel.g; if (dg<0) dg=-dg;
    int db=plte[2]-pixel.b; if (db<0) db=-db;
    int da;
    if (ix<trnsc) da=trns[ix]-pixel.a;
    else da=0xff-pixel.a;
    if (da<0) da=-da;
    int score=dr+dg+db+da;
    if (!score) return ix;
    if (score<bestscore) {
      bestix=ix;
      bestscore=score;
    }
  }
  return bestix;
}
 
static struct png_pixel png_read_i1(struct png_iterator *iter) {
  return png_ctlookup(iter,((*(iter->p))>>iter->shift)&1);
}

static void png_write_i1(struct png_iterator *iter,struct png_pixel pixel) {
  int ix=png_ctsearch(iter,pixel)&1;
  if (ix) (*(iter->p))|=(1<<iter->shift);
  else (*(iter->p))&=~(1<<iter->shift);
}
 
static struct png_pixel png_read_i2(struct png_iterator *iter) {
  return png_ctlookup(iter,((*(iter->p))>>iter->shift)&3);
}

static void png_write_i2(struct png_iterator *iter,struct png_pixel pixel) {
  int ix=png_ctsearch(iter,pixel)&3;
  (*(iter->p))=(((*(iter->p))&~(3<<iter->shift))|(ix<<iter->shift));
}
 
static struct png_pixel png_read_i4(struct png_iterator *iter) {
  return png_ctlookup(iter,((*(iter->p))>>iter->shift)&15);
}

static void png_write_i4(struct png_iterator *iter,struct png_pixel pixel) {
  int ix=png_ctsearch(iter,pixel)&15;
  if (iter->shift) (*(iter->p))=((*(iter->p))&0x0f)|(ix<<4);
  else (*(iter->p))=((*(iter->p))&0xf0)|ix;
}
 
static struct png_pixel png_read_i8(struct png_iterator *iter) {
  return png_ctlookup(iter,*(iter->p));
}

static void png_write_i8(struct png_iterator *iter,struct png_pixel pixel) {
  *iter->p=png_ctsearch(iter,pixel);
}

/* Pixel accessors: Luma+Alpha
 */
 
static struct png_pixel png_read_ya8(struct png_iterator *iter) {
  int luma=iter->p[0];
  luma|=luma<<8;
  int alpha=iter->p[1];
  alpha|=alpha<<8;
  return (struct png_pixel){
    .r=luma,
    .g=luma,
    .b=luma,
    .a=alpha,
  };
}

static void png_write_ya8(struct png_iterator *iter,struct png_pixel pixel) {
  iter->p[0]=((pixel.r+pixel.g+pixel.b)/3)>>8;
  iter->p[1]=pixel.a>>8;
}
 
static struct png_pixel png_read_ya16(struct png_iterator *iter) {
  int luma=(iter->p[0]<<8)|iter->p[1];
  return (struct png_pixel){
    .r=luma,
    .g=luma,
    .b=luma,
    .a=(iter->p[2]<<8)|iter->p[3],
  };
}

static void png_write_ya16(struct png_iterator *iter,struct png_pixel pixel) {
  int luma=(pixel.r+pixel.g+pixel.b)/3;
  iter->p[0]=luma>>8;
  iter->p[1]=luma;
  iter->p[2]=pixel.a>>8;
  iter->p[3]=pixel.a;
}

/* Pixel accessors: RGBA
 */
 
static struct png_pixel png_read_rgba8(struct png_iterator *iter) {
  return (struct png_pixel){
    .r=(iter->p[0]<<8)|iter->p[0],
    .g=(iter->p[1]<<8)|iter->p[1],
    .b=(iter->p[2]<<8)|iter->p[2],
    .a=(iter->p[3]<<8)|iter->p[3],
  };
}

static void png_write_rgba8(struct png_iterator *iter,struct png_pixel pixel) {
  iter->p[0]=pixel.r>>8;
  iter->p[1]=pixel.g>>8;
  iter->p[2]=pixel.b>>8;
  iter->p[3]=pixel.a>>8;
}
 
static struct png_pixel png_read_rgba16(struct png_iterator *iter) {
  return (struct png_pixel){
    .r=(iter->p[0]<<8)|iter->p[1],
    .g=(iter->p[2]<<8)|iter->p[3],
    .b=(iter->p[4]<<8)|iter->p[5],
    .a=(iter->p[6]<<8)|iter->p[7],
  };
}

static void png_write_rgba16(struct png_iterator *iter,struct png_pixel pixel) {
  iter->p[0]=pixel.r>>8;
  iter->p[1]=pixel.r;
  iter->p[2]=pixel.g>>8;
  iter->p[3]=pixel.g;
  iter->p[4]=pixel.b>>8;
  iter->p[5]=pixel.b;
  iter->p[6]=pixel.a>>8;
  iter->p[7]=pixel.a;
}

/* Initialize iterator.
 */

int png_iterate(struct png_iterator *iter,struct png_image *image) {
  if (!iter||!image) return -1;
  if ((image->w<1)||(image->h<1)) return -1;
  
  iter->image=image;
  iter->rowstart=iter->p=image->v;
  iter->xc=image->w-1; // (xc,yc) are how many more steps we can take, ie size-1
  iter->yc=image->h-1;
  iter->shift=8-image->pixelsize;
  iter->next=0;
  iter->read=0;
  iter->write=0;
  
  switch (image->colortype) {
    case 0: switch (image->depth) {
        case 1: iter->read=png_read_y1; iter->write=png_write_y1; break;
        case 2: iter->read=png_read_y2; iter->write=png_write_y2; break;
        case 4: iter->read=png_read_y4; iter->write=png_write_y4; break;
        case 8: iter->read=png_read_y8; iter->write=png_write_y8; break;
        case 16: iter->read=png_read_y16; iter->write=png_write_y16; break;
      } break;
    case 2: switch (image->depth) {
        case 8: iter->read=png_read_rgb8; iter->write=png_write_rgb8; break;
        case 16: iter->read=png_read_rgb16; iter->write=png_write_rgb16; break;
      } break;
    case 3: switch (image->depth) {
        case 1: iter->read=png_read_i1; iter->write=png_write_i1; break;
        case 2: iter->read=png_read_i2; iter->write=png_write_i2; break;
        case 4: iter->read=png_read_i4; iter->write=png_write_i4; break;
        case 8: iter->read=png_read_i8; iter->write=png_write_i8; break;
      } break;
    case 4: switch (image->depth) {
        case 8: iter->read=png_read_ya8; iter->write=png_write_ya8; break;
        case 16: iter->read=png_read_ya16; iter->write=png_write_ya16; break;
      } break;
    case 6: switch (image->depth) {
        case 8: iter->read=png_read_rgba8; iter->write=png_write_rgba8; break;
        case 16: iter->read=png_read_rgba16; iter->write=png_write_rgba16; break;
      } break;
  }
  switch (image->pixelsize) {
    case 1: iter->next=png_next_1; break;
    case 2: iter->next=png_next_2; break;
    case 4: iter->next=png_next_4; break;
    default: {
        if (image->pixelsize&7) return -1;
        iter->next=png_next_bytewise;
      }
  }
  if (!iter->read) return -1;
    
  return 0;
}

/* Convert from one image to another, of the same size.
 */
 
int png_image_convert(struct png_image *dst,const struct png_image *src) {
  if (!dst||!src||(dst->w!=src->w)||(dst->h!=src->h)) return -1;
  struct png_iterator dstiter,srciter;
  if (png_iterate(&dstiter,dst)<0) return -1;
  if (png_iterate(&srciter,(struct png_image*)src)<0) return -1;
  do {
    dstiter.write(&dstiter,srciter.read(&srciter));
  } while (dstiter.next(&dstiter)&&srciter.next(&srciter));
  return 0;
}

/* Reformat in place.
 */
 
struct png_image *png_image_reformat(const struct png_image *src,int depth,int colortype) {
  if (!src) return 0;
  struct png_image *dst=png_image_new(src->w,src->h,depth,colortype);
  if (!dst) return 0;
  
  /* If (dst) expects a color table, copy it from (src).
   */
  if (colortype==3) {
    const struct png_chunk *chunk=src->chunkv;
    int i=src->chunkc;
    for (;i-->0;chunk++) {
      if ((chunk->id==PNG_CHUNKID_PLTE)||(chunk->id==PNG_CHUNKID_tRNS)) {
        if (png_image_add_chunk(dst,chunk->id,chunk->v,chunk->c)<0) {
          png_image_del(dst);
          return 0;
        }
      }
    }
  }
  
  if (png_image_convert(dst,src)<0) {
    png_image_del(dst);
    return 0;
  }
  
  return dst;
}
