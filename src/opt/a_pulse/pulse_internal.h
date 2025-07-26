#ifndef PULSE_INTERNAL_H
#define PULSE_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include "opt/io/io.h"
#include "shovel/shovel.h"

#define UBUF_LIMIT 8

extern struct pulse {
  int rate,chanc,running;
  pthread_t iothd;
  pthread_mutex_t iomtx;
  int ioerror;
  int16_t *buf;
  int bufa; // samples
  pa_simple *pa;
  float *ubuf[UBUF_LIMIT]; // WEAK
  int ubufc;
  int ubuflen; // Maximum update we may ask for.
} pulse;

#endif
