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

extern struct g {
  const char *exename;
  const char *command;
  const char *dstpath;
  const char *srcpathv[SRCPATH_LIMIT];
  int srcpathc;
} g;

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

#endif
