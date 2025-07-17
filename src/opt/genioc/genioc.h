/* genioc.h
 * Generic inversion of control.
 * We provide main(), and defer to "io" unit for audio, video, and input.
 * Only MacOS does not use this because it has its own IoC regime.
 * We implement the Shovel Platform API, except the bits that "io" does.
 * We call shm_init, shm_update, and shm_quit. Audio unit calls sha_init and sha_update.
 */
 
#ifndef GENIOC_H
#define GENIOC_H

#endif
