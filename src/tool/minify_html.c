#include "tool_internal.h"
#include <stdarg.h>

/* Context.
 */
 
struct minify_html {
  struct sr_encoder *dst; // WEAK
  const char *src;
  int srcc;
  int srcp;
  const char *path;
  int logged_error;
};

static int minify_html_1(struct minify_html *ctx);

static void minify_html_cleanup(struct minify_html *ctx) {
}

/* Log error and return <0.
 * Only the first error will be logged, and only if a path was provided.
 * Provide (ref) if there's a specific point in the source where it happened, otherwise we use the context's position.
 */
 
static int fail(struct minify_html *ctx,const char *ref,const char *fmt,...) {
  if (ctx->logged_error) return ctx->logged_error;
  if (!ctx->path) return ctx->logged_error=-1;
  
  int lineno=0;
  if (ref) {
    int p=ref-ctx->src;
    if ((p>=0)&&(p<=ctx->srcc)) lineno=tool_lineno(ctx->src,p);
  }
  if (!lineno) lineno=tool_lineno(ctx->src,ctx->srcp);
  
  va_list vargs;
  va_start(vargs,fmt);
  char msg[256];
  int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
  if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
  while (msgc&&((unsigned char)msg[msgc-1]<=0x20)) msgc--; // In case I accidentally include an LF.
  
  fprintf(stderr,"%s:%d: %.*s\n",ctx->path,lineno,msgc,msg);
  
  return ctx->logged_error=-2;
}

/* Minify CSS.
 */
 
static int minify_css(struct minify_html *ctx,const char *src,int srcc) {
  // Not going to bother with anything here.
  // Our runtime has a meager amount of CSS that won't change much, might as well just minify it by hand.
  sr_encode_raw(ctx->dst,src,srcc);
  return 0;
}

/* Minify Javascript.
 * We're not going to do much.
 * My approach for this project has been to write it minifully in the first place.
 * So we'll just reduce whitespace and eliminate comments and call it a day.
 * Oh and also reduce literal integer sizes. I'm fond of hexadecimal with leading zeroes in source, but decimal tends to be shorter.
 */
 
static int minify_js(struct minify_html *ctx,const char *src,int srcc) {
  sr_encode_u8(ctx->dst,0x0a);
  int srcp=0,pvtokenc=0,linew=0;
  const char *pvtoken=0;
  while (srcp<srcc) {
    const char *token=src+srcp;
    int tokenc=js_token_measure(src+srcp,srcc-srcp);
    if (tokenc<1) return fail(ctx,token,"Javascript tokenization failed.");
    srcp+=tokenc;
    
    // Skip whitespace and comments. These do come back as regular tokens.
    if (((unsigned char)token[0]<=0x20)||((tokenc>=2)&&(token[0]=='/')&&((token[1]=='/')||(token[1]=='*')))) continue;
    
    // If we need a space, emit one.
    if (js_space_required(pvtoken,pvtokenc,token,tokenc)) {
      sr_encode_u8(ctx->dst,' ');
      linew++;
    }
    pvtoken=token;
    pvtokenc=tokenc;
    
    // Emit the token.
    sr_encode_raw(ctx->dst,token,tokenc);
    linew+=tokenc;
    
    // If we're above a certain width, insert LF after any token ending with semicolon or close bracket.
    // DO NOT just emit LFs anywhere between tokens! That might trigger semicolon insertion, and actually change the meaning of the code.
    // TODO It would be great if we could be even more judicious about these, eg after each class member or top-level function.
    // That would require more context than we have, and I'm not sure it's worth the considerable amount of effort.
    if ((linew>=100)&&((token[tokenc-1]==';')||(token[tokenc-1]=='}'))) {
      sr_encode_u8(ctx->dst,0x0a);
      linew=0;
      pvtoken=0;
      pvtokenc=0;
    }
  }
  sr_encode_u8(ctx->dst,0x0a);
  return 0;
}

/* Minify a run of HTML plain text.
 * Just condenses inner space.
 */
 
static int minify_html_text(struct minify_html *ctx,const char *src,int srcc) {
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { src++; srcc--; }
  int srcp=0;
  while (srcp<srcc) {
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    sr_encode_raw(ctx->dst,token,tokenc);
    if (srcp<srcc) sr_encode_u8(ctx->dst,0x20);
  }
  return 0;
}

/* Minify a singleton tag.
 */
 
static int minify_html_singleton(struct minify_html *ctx,struct html_token *token) {
  // TODO Inline CSS or favicon?
  // TODO Other semantic replacements?
  sr_encode_u8(ctx->dst,'<');
  sr_encode_raw(ctx->dst,token->name,token->namec);
  if (token->attrc) {
    sr_encode_u8(ctx->dst,' ');
    sr_encode_raw(ctx->dst,token->attr,token->attrc);
  }
  sr_encode_u8(ctx->dst,'/');
  sr_encode_u8(ctx->dst,'>');
  return 0;
}

/* Minify a nested tag, with body measured.
 */
 
static int minify_html_nested(struct minify_html *ctx,struct html_token *open,const char *body,int bodyc) {
  int err;

  /* Script, style, and our fake-script tag all do their own thing.
   */
  if ((open->namec==5)&&!memcmp(open->name,"style",5)) {
    sr_encode_raw(ctx->dst,"<style>",7);
    if ((err=minify_css(ctx,body,bodyc))<0) return err;
    sr_encode_raw(ctx->dst,"</style>",8);
    return 0;
  }
  if (
    ((open->namec==6)&&!memcmp(open->name,"script",6))||
    ((open->namec==3)&&!memcmp(open->name,"xyz",3))
  ) {
    sr_encode_u8(ctx->dst,'<');
    sr_encode_raw(ctx->dst,open->name,open->namec);
    if (open->attrc) { // TODO Check for [src] if we're allowing external scripts.
      sr_encode_u8(ctx->dst,' ');
      sr_encode_raw(ctx->dst,open->attr,open->attrc);
    }
    sr_encode_u8(ctx->dst,'>');
    if ((err=minify_js(ctx,body,bodyc))<0) return err;
    sr_encode_u8(ctx->dst,'<');
    sr_encode_u8(ctx->dst,'/');
    sr_encode_raw(ctx->dst,open->name,open->namec);
    sr_encode_u8(ctx->dst,'>');
    return 0;
  }
  
  /* Everything else, emit the open tag, then carefully fudge context to reenter body, then close it.
   */
  err=0;
  sr_encode_u8(ctx->dst,'<');
  sr_encode_raw(ctx->dst,open->name,open->namec);
  if (open->attrc) {
    sr_encode_u8(ctx->dst,' ');
    sr_encode_raw(ctx->dst,open->attr,open->attrc);
  }
  sr_encode_u8(ctx->dst,'>');
  int srcp0=ctx->srcp;
  int srcc0=ctx->srcc;
  ctx->srcp=body-ctx->src;
  if ((ctx->srcp<0)||(ctx->srcp>ctx->srcc)) { ctx->srcp=0; return -1; }
  ctx->srcc=ctx->srcp+bodyc;
  while (ctx->srcp<ctx->srcc) {
    if ((err=minify_html_1(ctx))<0) break;
  }
  ctx->srcp=srcp0;
  ctx->srcc=srcc0;
  sr_encode_u8(ctx->dst,'<');
  sr_encode_u8(ctx->dst,'/');
  sr_encode_raw(ctx->dst,open->name,open->namec);
  sr_encode_u8(ctx->dst,'>');
  return err;
}

/* Minify one stretch of HTML.
 * Must consume some input or fail.
 */
 
static int minify_html_1(struct minify_html *ctx) {
  int err;
  struct html_token token={0};
  int tokenc=html_token_next(&token,ctx->src+ctx->srcp,ctx->srcc-ctx->srcp);
  if (tokenc<0) return fail(ctx,0,"Malformed HTML.");
  if (!tokenc) {
    ctx->srcp=ctx->srcc;
    return 0;
  }
  switch (token.type) {
    case HTML_TOKEN_COMMENT: break;
    case HTML_TOKEN_PI: sr_encode_raw(ctx->dst,token.text,token.textc); break;
    case HTML_TOKEN_TEXT: if ((err=minify_html_text(ctx,token.text,token.textc))<0) return err; break;
    case HTML_TOKEN_CLOSE: return fail(ctx,token.text,"Unexpected closing tag '%.*s'.",token.namec,token.name);
    case HTML_TOKEN_SINGLETON: if ((err=minify_html_singleton(ctx,&token))<0) return err; break;
    case HTML_TOKEN_OPEN: { // Consume the body and closer, then call out.
        /* A problem: Our Javascript contains lots of '<', because "less than" is a perfectly normal thing to say in any code.
         * Browsers don't seem to mind the ambiguity, but our naive HTML parser chokes on it.
         * Solution: Get even naiver. Within a nesting block, we'll search for "<TAGNAME" and "</TAGNAME>" exactly.
         * Anything else is just text.
         */
        char opener[16],closer[16];
        int openerc=token.namec+1; // "<" only
        int closerc=token.namec+3; // "</" + ">"
        if (closerc>sizeof(closer)) return fail(ctx,0,"Tag name '%.*s' too long.",token.namec,token.name);
        opener[0]='<';
        memcpy(opener+1,token.name,token.namec);
        closer[0]='<';
        closer[1]='/';
        memcpy(closer+2,token.name,token.namec);
        closer[closerc-1]='>';
        const char *body=ctx->src+ctx->srcp+tokenc;
        int bodyc=0,depth=1;
        while (ctx->srcp+tokenc<ctx->srcc) {
          if ((ctx->srcp+tokenc<=ctx->srcc-openerc)&&!memcmp(ctx->src+ctx->srcp+tokenc,opener,openerc)) {
            depth++;
            tokenc+=openerc;
            bodyc+=openerc;
          } else if ((ctx->srcp+tokenc<=ctx->srcc-closerc)&&!memcmp(ctx->src+ctx->srcp+tokenc,closer,closerc)) {
            tokenc+=closerc;
            depth--;
            if (depth<=0) break;
            bodyc+=closerc;
          } else {
            bodyc++;
            tokenc++;
          }
        }
        if ((err=minify_html_nested(ctx,&token,body,bodyc))<0) return err;
      } break;
    default: return -1;
  }
  ctx->srcp+=tokenc;
  return 0;
}

/* Minify full HTML in context.
 */
 
static int minify_html_inner(struct minify_html *ctx) {
  while (ctx->srcp<ctx->srcc) {
    if (minify_html_1(ctx)<0) {
      return fail(ctx,0,"Unspecified error minifying HTML.");
    }
  }
  return 0;
}

/* Minify HTML, main entry point.
 */
 
int minify_html(struct sr_encoder *dst,const char *src,int srcc,const char *path) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  struct minify_html ctx={.dst=dst,.src=src,.srcc=srcc,.path=path};
  int err=minify_html_inner(&ctx);
  minify_html_cleanup(&ctx);
  if (err<0) return err;
  return sr_encoder_require(dst,0);
}
