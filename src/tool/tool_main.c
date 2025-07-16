#include "tool_internal.h"

struct g g={0};

/* Compile metadata.
 */
 
static int compile_metadata(struct sr_encoder *dst,const char *path,const char *src,int srcc) {
  int srcp=0,lineno=1;
  for (;srcp<srcc;lineno++) {
    const char *line=src+srcp;
    int linec=0;
    while ((srcp<srcc)&&(src[srcp++]!=0x0a)) linec++;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    if (!linec||(line[0]=='#')) continue;
    
    const char *k=line,*v=0;
    int kc=0,vc=0;
    while ((kc<linec)&&(k[kc]!='=')) kc++;
    if ((kc<linec)&&(k[kc]=='=')) {
      v=k+kc+1;
      vc=linec-kc-1;
      while (vc&&((unsigned char)v[vc-1]<=0x20)) vc--;
      while (vc&&((unsigned char)v[0]<=0x20)) { v++; vc--; }
    }
    while (kc&&((unsigned char)k[kc-1]<=0x20)) kc--;
    
    if (kc>0xff) {
      fprintf(stderr,"%s:%d: Key too long (%d, limit 255)\n",path,lineno,kc);
      return -2;
    }
    if (vc>0xff) {
      fprintf(stderr,"%s:%d: Value too long (%d, limit 255)\n",path,lineno,vc);
      return -2;
    }
    
    sr_encode_u8(dst,kc);
    sr_encode_u8(dst,vc);
    sr_encode_raw(dst,k,kc);
    sr_encode_raw(dst,v,vc);
  }
  return sr_encoder_require(dst,0);
}

/* Pack ROM to encoder with inputs in memory.
 */
 
static int pack_rom(
  struct sr_encoder *dst,
  const char *metapath,
  const char *metasrc,int metasrcc,
  const void *mainwasm,int mainwasmc,
  const void *audiowasm,int audiowasmc
) {
  int err;
  
  sr_encode_raw(dst,"\0SVL",4);
  sr_encode_intbe(dst,20,4); // TOC length, always 20.
  int metalenp=dst->c;
  sr_encode_zero(dst,4); // Placeholder for metadata length (we need to compile it first).
  sr_encode_intbe(dst,mainwasmc,4);
  sr_encode_intbe(dst,audiowasmc,4);
  
  int metastartp=dst->c;
  if ((err=compile_metadata(dst,metapath,metasrc,metasrcc))<0) return err;
  int metalen=dst->c-metastartp;
  ((unsigned char*)dst->v)[metalenp++]=metalen>>24;
  ((unsigned char*)dst->v)[metalenp++]=metalen>>16;
  ((unsigned char*)dst->v)[metalenp++]=metalen>>8;
  ((unsigned char*)dst->v)[metalenp]=metalen;
  
  sr_encode_raw(dst,mainwasm,mainwasmc);
  sr_encode_raw(dst,audiowasm,audiowasmc);
  return sr_encoder_require(dst,0);
}

/* Pack ROM, main.
 */
 
static int main_pack() {
  if (!g.dstpath) {
    fprintf(stderr,"%s: Please indicate output path with '-oPATH'\n",g.exename);
    return -2;
  }
  if (g.srcpathc!=3) {
    fprintf(stderr,"%s: Expected exactly 3 inputs (metadata,main.wasm,audio.wasm), found %d.\n",g.exename,g.srcpathc);
    return -2;
  }
  void *metasrc=0,*mainwasm=0,*audiowasm=0;
  int metasrcc=file_read(&metasrc,g.srcpathv[0]);
  if (metasrcc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",g.srcpathv[0]);
    return -2;
  }
  int mainwasmc=file_read(&mainwasm,g.srcpathv[1]);
  if (mainwasmc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",g.srcpathv[1]);
    free(metasrc);
    return -2;
  }
  int audiowasmc=file_read(&audiowasm,g.srcpathv[2]);
  if (audiowasmc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",g.srcpathv[2]);
    free(metasrc);
    free(mainwasm);
    return -2;
  }
  struct sr_encoder encoder={0};
  int err=pack_rom(&encoder,g.srcpathv[0],metasrc,metasrcc,mainwasm,mainwasmc,audiowasm,audiowasmc);
  if (err>=0) {
    if (file_write(g.dstpath,encoder.v,encoder.c)<0) {
      fprintf(stderr,"%s: Failed to write file, %d bytes.\n",g.dstpath,encoder.c);
      err=-2;
    }
  } else if (err!=-2) {
    fprintf(stderr,"%s: Unspecified error compiling ROM.\n",g.exename);
    err=-2;
  }
  free(metasrc);
  free(mainwasm);
  free(audiowasm);
  sr_encoder_cleanup(&encoder);
  return err;
}

/* Generate HTML.
 */
 
static int main_html() {
  if (!g.dstpath) {
    fprintf(stderr,"%s: Please indicate output path with '-oPATH'\n",g.exename);
    return -2;
  }
  if (g.srcpathc!=1) {
    fprintf(stderr,"%s: Expected exactly 1 input (template HTML), found %d.\n",g.exename,g.srcpathc);
    return -2;
  }
  char *html=0;
  int htmlc=file_read(&html,g.srcpathv[0]);
  if (htmlc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",g.srcpathv[0]);
    return -2;
  }
  struct sr_encoder dst={0};
  int err=minify_html(&dst,html,htmlc,g.srcpathv[0]);
  free(html);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error minifying html.\n",g.srcpathv[0]);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  if (file_write(g.dstpath,dst.v,dst.c)<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes.\n",g.dstpath,dst.c);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  sr_encoder_cleanup(&dst);
  return 0;
}

/* --help
 */
 
static void tool_print_help() {
  fprintf(stderr,"Usage: %s --help                        # Print this message.\n",g.exename);
  fprintf(stderr,"   Or: %s pack -oROM META MAIN AUDIO    # Make ROM file.\n",g.exename);
  fprintf(stderr,"   Or: %s html -oHTML TEMPLATE          # Make index.html.\n",g.exename);
}

/* Main.
 */
 
int main(int argc,char **argv) {

  g.exename="tool";
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) g.exename=argv[0];
  int argi=1;
  while (argi<argc) {
    const char *arg=argv[argi++];
    if (!arg||!arg[0]) continue;
    if (!strcmp(arg,"--help")) {
      tool_print_help();
      return 0;
    }
    if (!memcmp(arg,"-o",2)) {
      if (g.dstpath) {
        fprintf(stderr,"%s: Multiple output paths.\n",g.exename);
        return 1;
      }
      if (arg[2]) g.dstpath=arg+2;
      else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) g.dstpath=argv[argi++];
      else {
        fprintf(stderr,"%s: Argument required after '-o'\n",g.exename);
        return 1;
      }
      continue;
    }
    if (arg[0]=='-') {
      fprintf(stderr,"%s: Unexpected option '%s'\n",g.exename,arg);
      return 1;
    }
    if (!g.command) g.command=arg;
    else if (g.srcpathc<SRCPATH_LIMIT) g.srcpathv[g.srcpathc++]=arg;
    else {
      fprintf(stderr,"%s: Too many input paths, limit %d.\n",g.exename,SRCPATH_LIMIT);
      return 1;
    }
  }
  int err=-1;
  if (!g.command) { tool_print_help(); err=-2; }
  else if (!strcmp(g.command,"pack")) err=main_pack();
  else if (!strcmp(g.command,"html")) err=main_html();
  else {
    fprintf(stderr,"%s: Unknown command '%s'.\n",g.exename,g.command);
    err=-2;
  }
  if (err>=0) return 0;
  if (err!=-2) fprintf(stderr,"%s: Unspecified error running command '%s'.\n",g.exename,g.command);
  return 1;
}
