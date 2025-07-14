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

#endif
