#include "tool_internal.h"

/* Count newlines.
 */
 
int tool_lineno(const char *src,int srcc) {
  int lineno=1,srcp=0;
  for (;srcp<srcc;srcp++) {
    if (src[srcp]==0x0a) lineno++;
  }
  return lineno;
}

/* Parse HTML token.
 */
 
int html_token_next(struct html_token *token,const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp>=srcc) return 0; // Empty or all space: Report EOF.
  memset(token,0,sizeof(struct html_token));
  token->text=src+srcp;
  
  // If it doesn't start with '<', it's TEXT, up to the next '<' or EOF.
  if (src[srcp]!='<') {
    token->type=HTML_TOKEN_TEXT;
    while ((srcp<srcc)&&(src[srcp]!='<')) { srcp++; token->textc++; }
    while (token->textc&&((unsigned char)token->text[token->textc-1]<=0x20)) token->textc--;
    return srcp;
  }
  
  // COMMENT and PI behave similarly.
  if ((srcp<srcc-4)&&!memcmp(src+srcp,"<!--",4)) {
    token->type=HTML_TOKEN_COMMENT;
    token->textc=4;
    srcp+=4;
    for (;;) {
      if (srcp>srcc-3) return -1;
      if (!memcmp(src+srcp,"-->",3)) {
        srcp+=3;
        token->textc+=3;
        break;
      }
      srcp++;
      token->textc++;
    }
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    return srcp;
  }
  if ((srcp<srcc-2)&&!memcmp(src+srcp,"<!",2)) {
    token->type=HTML_TOKEN_PI;
    token->textc=2;
    srcp+=2;
    for (;;) {
      if (srcp>=srcc) return -1;
      token->textc++;
      if (src[srcp++]=='>') break;
    }
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    return srcp;
  }
  
  // OPEN, CLOSE, or SINGLETON. OPEN until we determine otherwise.
  token->type=HTML_TOKEN_OPEN;
  srcp++;
  token->textc++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) { srcp++; token->textc++; }
  if (srcp>=srcc) return -1;
  if (src[srcp]=='/') {
    token->type=HTML_TOKEN_CLOSE;
    srcp++;
    token->textc++;
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) { srcp++; token->textc++; }
  }
  token->name=src+srcp;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)&&(src[srcp]!='>')&&(src[srcp]!='/')) { srcp++; token->textc++; token->namec++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) { srcp++; token->textc++; }
  token->attr=src+srcp;
  while ((srcp<srcc)&&(src[srcp]!='>')&&(src[srcp]!='/')) { srcp++; token->textc++; token->attrc++; }
  while (token->attrc&&((unsigned char)token->attr[token->attrc-1]<=0x20)) token->attrc--;
  if (srcp>=srcc) return -1;
  if ((src[srcp]=='/')&&(token->type==HTML_TOKEN_OPEN)) {
    token->type=HTML_TOKEN_SINGLETON;
    srcp++;
    token->textc++;
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) { srcp++; token->textc++; }
    if (srcp>=srcc) return -1;
  }
  if (src[srcp]!='>') return -1;
  srcp++;
  token->textc++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  return srcp;
}

/* JS tokenization.
 */
 
static int js_isident(char ch) {
  if ((ch>='a')&&(ch<='z')) return 1;
  if ((ch>='A')&&(ch<='Z')) return 1;
  if ((ch>='0')&&(ch<='9')) return 1;
  if (ch=='_') return 1;
  // Javascript allows more characters in identifiers, a lot more. But luckily, I'm the one writing our JS and I keep it sane.
  return 0;
}

/* Measure JS token or space.
 */
 
int js_token_measure(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return 0;
  
  // Whitespace?
  if ((unsigned char)src[0]<=0x20) {
    int srcp=1;
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    return srcp;
  }
  
  // Comment?
  if ((src[0]=='/')&&(srcc>=2)) {
    if (src[1]=='/') {
      int srcp=2;
      while ((srcp<srcc)&&(src[srcp++]!=0x0a)) ;
      return srcp;
    }
    if (src[1]=='*') {
      int srcp=2;
      for (;;srcp++) {
        if (srcp>srcc-2) return -1; // Unclosed block comment.
        if (!memcmp(src+srcp,"*/",2)) return srcp+2;
      }
    }
  }
  
  // String?
  if ((src[0]=='"')||(src[0]=='\'')||(src[0]=='`')) {
    int srcp=1;
    for (;;) {
      if (srcp>=srcc) return -1; // Unclosed string.
      if (src[srcp]==0x0a) return -1; // LF in string. Legal for grave strings, but I won't do that.
      if (src[srcp]=='\\') srcp+=2;
      else if (src[srcp]==src[0]) return srcp+1;
      else srcp++;
    }
  }
  
  // We're not accomodating inline regexes. Just don't use them.
  
  // Identifiers and numbers.
  if (js_isident(src[0])) {
    int srcp=1;
    while ((srcp<srcc)&&js_isident(src[srcp])) srcp++;
    return srcp;
  }
  
  // Call everything else one byte of punctuation. Multibyte operators do exist but can be handled as separate tokens.
  return 1;
}

/* Determine whether whitespace is required.
 */
 
int js_space_required(const char *a,int ac,const char *b,int bc) {
  if ((ac<1)||(bc<1)) return 0;
  return js_isident(a[ac-1])&&js_isident(b[0]);
  // There are other possible space-required constructions, eg "a+ +b", but I don't think any will come up.
}
