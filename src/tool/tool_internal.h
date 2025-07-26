#ifndef TOOL_INTERNAL_H
#define TOOL_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"

#define SRCPATH_LIMIT 16
#define ARGV_LIMIT 16

extern struct g {
  const char *exename;
  const char *command;
  const char *dstpath;
  const char *srcpathv[SRCPATH_LIMIT];
  int srcpathc;
  const char *argv[ARGV_LIMIT]; // Only arguments not consumed above; usually empty.
  int argc;
} g;

int tool_arg_string(void *dstpp,const char *k,int kc);
int tool_arg_int(const char *k,int kc,int fallback);

int minify_html(struct sr_encoder *dst,const char *src,int srcc,const char *path);

int tool_lineno(const char *src,int srcc);

/* Measure an atomic token of HTML. No nesting.
 * Leading and trailing whitespace are quietly consumed.
 * Returns <0 for errors, >0 (length consumed) if a token was produced, or 0 at EOF.
 * Beware that 0 is possible for non-empty input, if it's all space.
 */
#define HTML_TOKEN_COMMENT   1 /* Plain comments only. */
#define HTML_TOKEN_PI        2 /* Other "<!" constructions. */
#define HTML_TOKEN_TEXT      3 /* Plain text. No leading or trailing space. May contain escapes. */
#define HTML_TOKEN_SINGLETON 4 /* Explicit singleton tag. We don't do named void elements. (name,attr) */
#define HTML_TOKEN_OPEN      5 /* Open tag. (name,attr) */
#define HTML_TOKEN_CLOSE     6 /* Close tag. (name) */
struct html_token {
  int type;
  const char *text,*name,*attr;
  int textc,namec,attrc;
};
int html_token_next(struct html_token *token,const char *src,int srcc);

int js_token_measure(const char *src,int srcc);
int js_space_required(const char *a,int ac,const char *b,int bc); // Do we need a space to distinguish these tokens?

/* Anything-to-anything conversion.
 * You must provide both paths, to hint at the desired format. Empty or some dummy if you need to.
 */
int tool_convert(struct sr_encoder *dst,const void *src,int srcc,const char *dstpath,const char *srcpath);

#define TOOL_FMT_UNKNOWN 0
#define TOOL_FMT_PNG     1
#define TOOL_FMT_C       2 /* Image as C source. */
#define TOOL_FMT_RAWIMG  3 /* Just the pixels of an image, binary. No dimensions. Suffixes: y1 y2 y4 y8 y16 i1 i2 i4 i8 ya8 ya16 rgb8 rgb16 rgba8 rgba16 */
#define TOOL_FMT_MIDI    4
#define TOOL_FMT_SYNMIN  5

// If (pixfmt) is insufficient, we'll mismatch and return zero. Should be at least 6.
int tool_split_rawimg_fmt(char *pixfmt,int pixfmta,int *chsize,const char *path);

// Empty if we can't find one; caller should make up a default then.
int tool_c_object_name(char *dst,int dsta,const char *srcpath,const char *dstpath);

int tool_convert_png(struct sr_encoder *dst,const void *src,int srcc,const char *path);
int tool_convert_c_png(struct sr_encoder *dst,const void *src,int srcc,const char *path,const char *dstpath);
int tool_convert_rawimg_png(struct sr_encoder *dst,const void *src,int srcc,const char *path,const char *dstpath);
int tool_convert_midi(struct sr_encoder *dst,const void *src,int srcc,const char *path);
int tool_convert_synmin_midi(struct sr_encoder *dst,const void *src,int srcc,const char *path);
int tool_convert_c_midi(struct sr_encoder *dst,const void *src,int srcc,const char *path,const char *dstpath);
int tool_convert_c_any(struct sr_encoder *dst,const void *src,int srcc,const char *path,const char *dstpath); // Raw data, as an array of bytes.

#endif
