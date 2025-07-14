#include "shovel/shovel.h"

/* Quit.
 */
 
void shm_quit(int status) {
  shovel_log("shm_quit");
}

/* Init.
 */

int shm_init() {
  shovel_log("shm_init");
  return 0;
}

/* Update.
 */

void shm_update(double elapsed) {
  shovel_log("shm_update, terminating");
  shovel_terminate(0);
}
