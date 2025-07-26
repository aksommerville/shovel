#include "tool_internal.h"

/* Object name for C file production.
 */
 
int tool_c_object_name(char *dst,int dsta,const char *srcpath,const char *dstpath) {
  const char *name=0;
  int namec=0;
  if (dstpath) {
    int pathp=0,stop=0;
    for (;dstpath[pathp];pathp++) {
      if (dstpath[pathp]=='/') {
        name=dstpath+pathp+1;
        namec=0;
        stop=0;
      } else if (dstpath[pathp]=='.') {
        stop=1;
      } else if (name&&!stop) {
        namec++;
      }
    }
    if (namec) {
      if (namec<=dsta) memcpy(dst,name,namec);
      if (namec<dsta) dst[namec]=0;
      return namec;
    }
  }
  if (srcpath&&!namec) {
    int pathp=0,stop=0;
    for (;srcpath[pathp];pathp++) {
      if (srcpath[pathp]=='/') {
        name=srcpath+pathp+1;
        namec=0;
        stop=0;
      } else if (srcpath[pathp]=='.') {
        stop=1;
      } else if (name&&!stop) {
        namec++;
      }
    }
    if (namec) {
      if (namec<=dsta) memcpy(dst,name,namec);
      if (namec<dsta) dst[namec]=0;
      return namec;
    }
  }
  return 0;
}

/* Read the multitude of rawimg suffixes.
 */
 
int tool_split_rawimg_fmt(char *pixfmt,int pixfmta,int *chsize,const char *path) {
  if (!path) return 0;
  const char *tail=0;
  int extc=0,i=0;
  for (;path[i];i++) {
    if (path[i]=='.') {
      tail=path+i+1;
      extc=0;
    } else if (tail) {
      extc++;
    }
  }
  if ((extc<2)||(extc>pixfmta)) return 0;
  i=extc;
  while (i-->0) {
    if ((tail[i]>='A')&&(tail[i]<='Z')) pixfmt[i]=tail[i]+0x20;
    else pixfmt[i]=tail[i];
  }
  *chsize=0;
  int shift=1;
  while (extc&&(pixfmt[extc-1]>='0')&&(pixfmt[extc-1]<='9')) {
    extc--;
    int digit=pixfmt[extc]-'0';
    (*chsize)+=digit*shift;
    shift*=10;
  }
  if ((*chsize<1)||(*chsize>64)||!extc) return 0;
  if ((extc==1)&&(pixfmt[0]=='y')) return extc;
  if ((extc==1)&&(pixfmt[0]=='i')) return extc;
  if ((extc==1)&&(pixfmt[0]=='a')) return extc;
  if ((extc==2)&&!memcmp(pixfmt,"ya",2)) return extc;
  if ((extc==3)&&!memcmp(pixfmt,"rgb",3)) return extc;
  if ((extc==4)&&!memcmp(pixfmt,"rgba",4)) return extc;
  return 0;
}

/* Format for known suffixes.
 */
 
static int tool_format_from_suffix(const char *path) {

  // Copy some fixed amount off the end and force it lowercase.
  // We're not just chopping at the last dot, in case we want to support things like ".tar.gz".
  if (!path) return TOOL_FMT_UNKNOWN;
  int pathc=0;
  while (path[pathc]) pathc++;
  char tail[16];
  int tailc=pathc;
  if (tailc>sizeof(tail)) tailc=sizeof(tail);
  int i=tailc,pathp=pathc;
  while (i-->0) {
    pathp--;
    if ((path[pathp]>='A')&&(path[pathp]<='Z')) tail[i]=path[pathp]+0x20;
    else tail[i]=path[pathp];
  }
  
  // Mostly we match literal suffixes.
  #define CK(ext,fmt) if ((tailc>=sizeof(ext)-1)&&!memcmp(tail+tailc-sizeof(ext)+1,ext,sizeof(ext)-1)) return TOOL_FMT_##fmt;
  CK(".png",PNG)
  CK(".c",C)
  CK(".mid",MIDI)
  #undef CK
  
  // rawimg does its own thing.
  char pixfmt[16];
  int chsize;
  if (tool_split_rawimg_fmt(pixfmt,sizeof(pixfmt),&chsize,path)>0) return TOOL_FMT_RAWIMG;
  
  return TOOL_FMT_UNKNOWN;
}

/* Guess format for input.
 */
 
static int tool_guess_format(const uint8_t *src,int srcc,const char *path) {
  int fmt=tool_format_from_suffix(path);
  if (fmt!=TOOL_FMT_UNKNOWN) return fmt;
  if ((srcc>=8)&&!memcmp(src,"\x89PNG\r\n\x1a\n",8)) return TOOL_FMT_PNG;
  if ((srcc>=8)&&!memcmp(src,"MThd\0\0\0\6",8)) return TOOL_FMT_MIDI;
  return TOOL_FMT_UNKNOWN;
}

/* Guess format for output.
 */
 
static int tool_guess_output_format(int srcfmt,const char *path) {
  int fmt=tool_format_from_suffix(path);
  if (fmt!=TOOL_FMT_UNKNOWN) return fmt;
  // Is there a default expected output format per input format?
  switch (srcfmt) {
    // ...hmm, no. Maybe we should default to C? I'm thinking embed-in-executable will be the only means of data delivery.
  }
  return TOOL_FMT_UNKNOWN;
}

/* Convert.
 */
 
int tool_convert(struct sr_encoder *dst,const void *src,int srcc,const char *dstpath,const char *srcpath) {
  if (!dst||(srcc<0)||(srcc&&!src)||!dstpath||!srcpath) return -1;
  int srcfmt=tool_guess_format(src,srcc,srcpath);
  int dstfmt=tool_guess_output_format(srcfmt,dstpath);
  
  /* Dispatch on formats.
   * If a conversion exists, call it and return. Fall thru if not.
   * Things I've marked "doable" here could be implemented but I don't expect to need them.
   */
  switch (dstfmt) {
  
    case TOOL_FMT_UNKNOWN: {
        fprintf(stderr,"%s:WARNING: Unable to guess output format. Copying %s verbatim.\n",dstpath,srcpath);
        return sr_encode_raw(dst,src,srcc);
      }
      
    case TOOL_FMT_PNG: switch (srcfmt) {
        case TOOL_FMT_PNG: return tool_convert_png(dst,src,srcc,srcpath);
        case TOOL_FMT_C: break; // doable
        case TOOL_FMT_RAWIMG: break; // tricky: Input doesn't declare its dimensions.
      } break;
      
    case TOOL_FMT_C: switch (srcfmt) {
        case TOOL_FMT_PNG: return tool_convert_c_png(dst,src,srcc,srcpath,dstpath);
        case TOOL_FMT_C: return sr_encode_raw(dst,src,srcc);
        case TOOL_FMT_MIDI: return tool_convert_c_midi(dst,src,srcc,srcpath,dstpath);
        default: return tool_convert_c_any(dst,src,srcc,srcpath,dstpath);
      } break;
      
    case TOOL_FMT_RAWIMG: switch (srcfmt) {
        case TOOL_FMT_PNG: return tool_convert_rawimg_png(dst,src,srcc,srcpath,dstpath);
        case TOOL_FMT_C: break; // doable
        case TOOL_FMT_RAWIMG: return sr_encode_raw(dst,src,srcc);
      } break;
      
    case TOOL_FMT_MIDI: switch (srcfmt) {
        case TOOL_FMT_MIDI: return tool_convert_midi(dst,src,srcc,srcpath);
        case TOOL_FMT_SYNMIN: break; // doable
        case TOOL_FMT_C: break; // doable
      } break;
      
    case TOOL_FMT_SYNMIN: switch (srcfmt) {
        case TOOL_FMT_MIDI: return tool_convert_synmin_midi(dst,src,srcc,srcpath);
      } break;
      
    default: {
        fprintf(stderr,"%s: Unknown output format %d [%s:%d]\n",dstpath,dstfmt,__FILE__,__LINE__);
        return -2;
      }
  }
  if (srcfmt==TOOL_FMT_UNKNOWN) {
    fprintf(stderr,"%s: Unknown format.\n",srcpath);
    return -2;
  }
  fprintf(stderr,"%s: Conversion %d=>%d not implemented\n",srcpath,srcfmt,dstfmt);
  return -2;
}

/* C from raw data.
 */
 
int tool_convert_c_any(struct sr_encoder *dst,const void *src,int srcc,const char *path,const char *dstpath) {
  char name[64];
  int namec=tool_c_object_name(name,sizeof(name),path,dstpath);
  if ((namec<1)||(namec>sizeof(name))) {
    memcpy(name,"object",6);
    namec=6;
  }
  sr_encode_fmt(dst,"const int %.*s_len=%d;\n",namec,name,srcc);
  sr_encode_fmt(dst,"const unsigned char %.*s[]={\n",namec,name);
  int linestart=dst->c;
  const uint8_t *p=src;
  int c=srcc;
  for (;c-->0;p++) {
    sr_encode_fmt(dst,"%d,",*p);
    if (dst->c-linestart>=100) {
      sr_encode_u8(dst,0x0a);
      linestart=dst->c;
    }
  }
  sr_encode_raw(dst,"};\n",3);
  return sr_encoder_require(dst,0);
}
