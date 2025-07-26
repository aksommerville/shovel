#ifndef SYNMIN_INTERNAL_H
#define SYNMIN_INTERNAL_H

#include "synmin.h"

// Feel free to modify these, they're just memory constraints.
#define SYNMIN_VOICE_LIMIT 16

struct synmin_voice {
  int ttl; // Frames; 0=defunct
  float level;
  int p; // full range
  int dp;
  int ddp;
};

extern struct synmin {
  int rate;
  int ratev[64]; // Delta per noteid.
  struct synmin_voice voicev[SYNMIN_VOICE_LIMIT];
  int voicec;
  const unsigned char *song;
  int songp,songc;
  int songdelay;
  int songrepeat;
} synmin;

#endif
