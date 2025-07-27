#include "xegl.h"
#include "shovel/shovel.h"
#include "opt/io/io.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h> // mswin needs this explicitly included; linux does not

const char *io_video_driver_name="xegl";
const int io_video_driver_provides_events=1;

static struct {
  struct xegl *xegl;
  const void *fb;
  int fbw,fbh;
  int w,h;
  void (*cb_close)();
  void (*cb_focus)(int focus);
  GLuint texid;
  int programid;
} v_xegl={0};

/* Quit.
 */
 
void io_video_quit() {
  if (v_xegl.texid) glDeleteTextures(1,&v_xegl.texid);
  if (v_xegl.programid) glDeleteProgram(v_xegl.programid);
  xegl_del(v_xegl.xegl);
  memset(&v_xegl,0,sizeof(v_xegl));
}

/* Callbacks.
 */
 
static void cb_close(void *userdata) {
  if (v_xegl.cb_close) v_xegl.cb_close();
}

static void cb_focus(void *userdata,int focus) {
  if (v_xegl.cb_focus) v_xegl.cb_focus(focus);
}

static void cb_resize(void *userdata,int w,int h) {
  if ((w<1)||(h<1)) return;
  v_xegl.w=w;
  v_xegl.h=h;
}

static int cb_key(void *userdata,int keycode,int value) {
  if (value==2) return 1;
  io_input_set_key(keycode,value);
  return 1;
}

/* GLSL.
 */
 
static const char vshader[]=
  "precision mediump float;\n"
  "attribute vec2 aposition;\n"
  "attribute vec2 atexcoord;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
  "  gl_Position=vec4(aposition,0.0,1.0);\n"
  "  vtexcoord=atexcoord;\n"
  "}\n"
"";

static const char fshader[]=
  "precision mediump float;\n"
  "uniform sampler2D sampler;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
  "  gl_FragColor=texture2D(sampler,vtexcoord);\n"
  "}\n"
"";

/* Compile shader.
 */
 
static int compile_shader(const char *src,int srcc,int type) {

  char version[256];
  int versionc;
  versionc=snprintf(version,sizeof(version),"#version %d\n",100/*eh.glsl_version*/);
  if ((versionc<1)||(versionc>=sizeof(version))) return -1;
  const char *srcv[2]={version,src};
  GLint lenv[2]={versionc,srcc};

  GLuint id=glCreateShader(type);
  if (!id) return -1;
  glShaderSource(id,2,srcv,lenv);
  glCompileShader(id);

  GLint status=0;
  glGetShaderiv(id,GL_COMPILE_STATUS,&status);
  if (status) {
    glAttachShader(v_xegl.programid,id);
    glDeleteShader(id);
    return 0;
  }

  int err=-1;
  GLint loga=0;
  glGetShaderiv(id,GL_INFO_LOG_LENGTH,&loga);
  if ((loga>0)&&(loga<INT_MAX)) {
    char *log=malloc(loga);
    if (log) {
      GLint logc=0;
      glGetShaderInfoLog(id,loga,&logc,log);
      while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
      if ((logc>0)&&(logc<=loga)) {
        fprintf(stderr,"Error compiling %s shader:\n%.*s\n",(type==GL_VERTEX_SHADER)?"vertex":"fragment",logc,log);
        err=-2;
      }
      free(log);
    }
  }
  glDeleteShader(id);
  return err;
}

/* Link shader.
 */
 
static int link_shader() {

  glLinkProgram(v_xegl.programid);
  GLint status=0;
  glGetProgramiv(v_xegl.programid,GL_LINK_STATUS,&status);
  if (status) return 0;

  /* Link failed. */
  int err=-1;
  GLint loga=0;
  glGetProgramiv(v_xegl.programid,GL_INFO_LOG_LENGTH,&loga);
  if ((loga>0)&&(loga<INT_MAX)) {
    char *log=malloc(loga);
    if (log) {
      GLint logc=0;
      glGetProgramInfoLog(v_xegl.programid,loga,&logc,log);
      while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
      if ((logc>0)&&(logc<=loga)) {
        fprintf(stderr,"Error linking shader:\n%.*s\n",logc,log);
        err=-2;
      }
      free(log);
    }
  }
  return err;
}

/* Initialize shader.
 */
 
static int init_shader() {

  if (!(v_xegl.programid=glCreateProgram())) return -1;
  if (compile_shader(vshader,sizeof(vshader),GL_VERTEX_SHADER)<0) return -1;
  if (compile_shader(fshader,sizeof(fshader),GL_FRAGMENT_SHADER)<0) return -1;
  
  glBindAttribLocation(v_xegl.programid,0,"aposition");
  glBindAttribLocation(v_xegl.programid,1,"atexcoord");
  
  if (link_shader()<0) return -1;
  
  glUseProgram(v_xegl.programid);

  return 0;
}

/* Init.
 */

int io_video_init(struct io_video_setup *setup) {
  if (v_xegl.xegl) return -1;
  struct xegl_delegate delegate={
    .cb_close=cb_close,
    .cb_focus=cb_focus,
    .cb_resize=cb_resize,
    .cb_key=cb_key,
  };
  struct xegl_setup xsetup={
    .title=setup->title,
    .iconrgba=setup->iconrgba,
    .iconw=setup->iconw,
    .iconh=setup->iconh,
    .w=setup->w,
    .h=setup->h,
    .fullscreen=setup->fullscreen,
    .fbw=setup->fbw,
    .fbh=setup->fbh,
    .device=0,
  };
  if (!(v_xegl.xegl=xegl_new(&delegate,&xsetup))) return -1;
  v_xegl.cb_close=setup->cb_close;
  v_xegl.cb_focus=setup->cb_focus;
  xegl_get_size(&v_xegl.w,&v_xegl.h,v_xegl.xegl);
  if (v_xegl.w<1) v_xegl.w=1;
  if (v_xegl.h<1) v_xegl.h=1;
  
  if (init_shader()<0) return -1;
  glGenTextures(1,&v_xegl.texid);
  glBindTexture(GL_TEXTURE_2D,v_xegl.texid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  
  return 0;
}

/* Update.
 */

int io_video_update() {
  return xegl_update(v_xegl.xegl);
}

/* Receive framebuffer.
 */

void sh_fb(const void *rgbx,int w,int h) {
  if ((w<1)||(h<1)) return;
  v_xegl.fb=rgbx;
  v_xegl.fbw=w;
  v_xegl.fbh=h;
}

/* Commit frame.
 */
 
int io_video_commit() {
  if (!v_xegl.fb) return 0;
  xegl_begin(v_xegl.xegl);

  glViewport(0,0,v_xegl.w,v_xegl.h);
  glClearColor(0.0f,0.0f,0.0f,1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D,v_xegl.texid);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,v_xegl.fbw,v_xegl.fbh,0,GL_RGBA,GL_UNSIGNED_BYTE,v_xegl.fb);
  
  // The scale-up decision isn't likely to change one frame to the next, but it's a fairly cheap calculation so whatever.
  int xscale=v_xegl.w/v_xegl.fbw;
  int yscale=v_xegl.h/v_xegl.fbh;
  int scale=(xscale<yscale)?xscale:yscale;
  if (scale<1) scale=1;
  //TODO Could add a bit, if (scale>4) or so, scale all the way up. Sometimes I do that. I'm on the fence whether the disparate pixel sizes is a problem at those larger scales.
  // (fwiw, i've been on that fence for like twenty years, so don't expect an answer from this quarter :P )
  int dstw=v_xegl.fbw*scale;
  int dsth=v_xegl.fbh*scale;
  GLfloat r=(GLfloat)dstw/(GLfloat)v_xegl.w;
  GLfloat t=(GLfloat)dsth/(GLfloat)v_xegl.h;
  struct { GLfloat x,y; } positionv[]={
    {-r, t},
    {-r,-t},
    { r, t},
    { r,-t},
  };
  struct { GLubyte x,y; } texcoordv[]={
    {0,0},{0,1},{1,0},{1,1},
  };
  
  glUseProgram(v_xegl.programid);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_FLOAT,0,sizeof(positionv[0]),positionv);
  glVertexAttribPointer(1,2,GL_UNSIGNED_BYTE,0,sizeof(texcoordv[0]),texcoordv);
  glDrawArrays(GL_TRIANGLE_STRIP,0,4);

  xegl_end(v_xegl.xegl);
  v_xegl.fb=0;
  return 0;
}
