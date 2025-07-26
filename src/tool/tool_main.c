#include "tool_internal.h"

struct g g={0};

/* Pack ROM to encoder with inputs in memory.
 */
 
static int pack_rom(
  struct sr_encoder *dst,
  const void *mainwasm,int mainwasmc,
  const void *audiowasm,int audiowasmc
) {
  int err;
  
  sr_encode_raw(dst,"\0SVL",4);
  sr_encode_intbe(dst,16,4); // TOC length, always 16.
  sr_encode_intbe(dst,mainwasmc,4);
  sr_encode_intbe(dst,audiowasmc,4);
  
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
  if (g.srcpathc!=2) {
    fprintf(stderr,"%s: Expected exactly 2 inputs (main.wasm,audio.wasm), found %d.\n",g.exename,g.srcpathc);
    return -2;
  }
  void *mainwasm=0,*audiowasm=0;
  int mainwasmc=file_read(&mainwasm,g.srcpathv[0]);
  if (mainwasmc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",g.srcpathv[0]);
    return -2;
  }
  int audiowasmc=file_read(&audiowasm,g.srcpathv[1]);
  if (audiowasmc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",g.srcpathv[1]);
    free(mainwasm);
    return -2;
  }
  struct sr_encoder encoder={0};
  int err=pack_rom(&encoder,mainwasm,mainwasmc,audiowasm,audiowasmc);
  if (err>=0) {
    if (file_write(g.dstpath,encoder.v,encoder.c)<0) {
      fprintf(stderr,"%s: Failed to write file, %d bytes.\n",g.dstpath,encoder.c);
      err=-2;
    }
  } else if (err!=-2) {
    fprintf(stderr,"%s: Unspecified error compiling ROM.\n",g.exename);
    err=-2;
  }
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

/* Convert file.
 */
 
static int main_convert() {
  if (!g.dstpath||(g.srcpathc!=1)) {
    fprintf(stderr,"%s: 'convert' requires exactly one input and output path.\n",g.exename);
    return -2;
  }
  void *src=0;
  int srcc=file_read(&src,g.srcpathv[0]);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",g.srcpathv[0]);
    return -2;
  }
  struct sr_encoder dst={0};
  int err=tool_convert(&dst,src,srcc,g.dstpath,g.srcpathv[0]);
  free(src);
  if (err<0) {
    sr_encoder_cleanup(&dst);
    if (err!=-2) fprintf(stderr,"%s: Unspecified error converting (dstpath='%s')\n",g.srcpathv[0],g.dstpath);
    return -2;
  }
  err=file_write(g.dstpath,dst.v,dst.c);
  sr_encoder_cleanup(&dst);
  if (err<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes.\n",g.dstpath,dst.c);
    return -2;
  }
  return 0;
}

/* --help
 */
 
static void tool_print_help() {
  fprintf(stderr,"\n");
  fprintf(stderr,"Usage: %s --help                   # Print this message.\n",g.exename);
  fprintf(stderr,"   Or: %s pack -oROM MAIN AUDIO    # Make ROM file.\n",g.exename);
  fprintf(stderr,"   Or: %s html -oHTML TEMPLATE     # Make index.html.\n",g.exename);
  fprintf(stderr,"   Or: %s convert -oDST SRC ...    # General data conversion.\n",g.exename);
  fprintf(stderr,"\n");
  fprintf(stderr,"Options for 'convert':\n");
  fprintf(stderr,"  --depth=INTEGER         # To/from PNG\n");
  fprintf(stderr,"  --colortype=INTEGER     # To/from PNG\n");
  fprintf(stderr,"  --synth=synmin|midi     # C<=MIDI\n");
  fprintf(stderr,"\n");
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
      if (g.argc>=ARGV_LIMIT) {
        fprintf(stderr,"%s: Too many arguments.\n",g.exename);
        return 1;
      }
      while (arg[0]=='-') arg++;
      g.argv[g.argc++]=arg;
      continue;
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
  else if (!strcmp(g.command,"convert")) err=main_convert();
  else {
    fprintf(stderr,"%s: Unknown command '%s'.\n",g.exename,g.command);
    err=-2;
  }
  if (err>=0) return 0;
  if (err!=-2) fprintf(stderr,"%s: Unspecified error running command '%s'.\n",g.exename,g.command);
  return 1;
}

/* Get loose arguments.
 */
 
int tool_arg_string(void *dstpp,const char *k,int kc) {
  if (!k) return 0;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!kc) return 0;
  const char **arg=g.argv;
  int i=g.argc;
  for (;i-->0;arg++) {
    if (memcmp(*arg,k,kc)) continue;
    if (!(*arg)[kc]) {
      *(const void**)dstpp="1";
      return 1;
    }
    if ((*arg)[kc]=='=') {
      const char *v=(*arg)+kc+1;
      int vc=0;
      while (v[vc]) vc++;
      *(const void**)dstpp=v;
      return vc;
    }
  }
  return 0;
}

int tool_arg_int(const char *k,int kc,int fallback) {
  const char *v=0;
  int vc=tool_arg_string(&v,k,kc);
  if (vc<1) return fallback;
  int vn=0;
  int i=0;
  for (;i<vc;i++) {
    int digit=v[i]-'0';
    if ((digit<0)||(digit>9)) return fallback;
    vn*=10;
    vn+=digit;
  }
  return vn;
}
