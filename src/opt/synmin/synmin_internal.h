#ifndef SYNMIN_INTERNAL_H
#define SYNMIN_INTERNAL_H

#include "synmin.h"

// Feel free to modify these, they're just memory constraints.
#define SYNMIN_VOICE_LIMIT 16

struct synmin_voice {
  int ttl; // Frames; 0=defunct
  int p; // Frames.
  int halftime; // (p>=halftime), we emit low instead of high.
  int fulltime; // (p>=fulltime), (p) resets to zero.
  int adjtime; // Frames, constant, adjust (halftime,fulltime) so often.
  int adjclock; // Counts down from adjtime.
  int adjd; // How much to adjust fulltime.
  float level;
};

extern struct synmin {
  int rate;
  int periodv[64]; // Indexed by noteid, calculated at init. All >=2.
  struct synmin_voice voicev[SYNMIN_VOICE_LIMIT];
  int voicec;
  const unsigned char *song;
  int songp,songc;
  int songdelay;
  int songrepeat;
} synmin;

#endif
