/* io.h
 * Generic abstraction for video, audio, and input drivers.
 * Our native builds have a single-driver model: You pick a driver at build time, and it's the only one.
 * That's considerably less flexible than my usual pick-at-runtime model, but also much simpler.
 * The goal for shovel is to keep things as small as possible.
 *
 * The interfaces declared here are implemented by driver units.
 * You must select exactly one each of audio, video, and input.
 * 'macos' and 'mswin' provide all three.
 * The more a-la-carte units are named "a_*" for audio, "v_*" for video, and "i_*" for input.
 */
 
#ifndef IO_H
#define IO_H

/* Video.
 *******************************************************************/
 
struct io_video_setup {
  int w,h; // Initial size of window, typically zero for auto.
  int fbw,fbh; // Expected framebuffer size, to guide automatic window size.
  int fullscreen;
  const char *title;
  const void *iconrgba;
  int iconw,iconh;
  
  /* The only callbacks are close and focus.
   * Raw keyboard events should be tracked if available; driver should deliver them directly to io_input.
   * Mouse events should not be tracked.
   * Video drivers do not necessarily provide events at all, eg DRM won't.
   */
  void (*cb_close)();
  void (*cb_focus)(int focus);
};

/* Drivers must tolerate redundant quit calls.
 */
void io_video_quit();

int io_video_init(struct io_video_setup *setup);

/* Platform calls at the start of each cycle.
 * Output-only drivers should noop.
 * Others should poll for events and callback as warranted.
 */
int io_video_update();

/* This bit of the Shovel Platform API must be implemented by the video unit.
 * Grab the buffer and size, and hold on to that until commit.
 * Do not copy pixels at this time, content is allowed to change before commit.
 */
void sh_fb(const void *rgbx,int w,int h);

/* Outer platform calls after each client update.
 * If sh_fb() hasn't been called, do nothing, keep the prior image.
 * Otherwise, commit that last sh_fb() call to the output.
 */
int io_video_commit();

extern const char *io_video_driver_name;
extern const int io_video_provides_events; // Nonzero if (io_video_setup.cb_*) may be called.

/* Audio.
 ******************************************************************/
 
struct io_audio_setup {
  int rate,chanc;
  const char *device;
  int buffer;
};

/* Drivers must tolerate redundant quit calls.
 */
void io_audio_quit();

/* Prepares the driver, and also runs sha_init().
 */
int io_audio_init(struct io_audio_setup *setup);

/* Newly-initialized drivers should be stopped.
 * Platform will explicitly set_running(1) when we're ready to start producing sound.
 * Platform should also stop playback when the app loses focus.
 */
int io_audio_set_running(int running);
int io_audio_get_running();

/* Most audio drivers are expected to run their own I/O thread.
 * In that case, update should only check for errors.
 * Drivers are allowed to operate synchronously, and do all their work during update (but not recommended).
 * Either way, the audio unit alone is responsible for calling sha_update().
 */
int io_audio_update();

/* Driver must guarantee that the game's audio segment is not running while you hold this lock.
 * Platform will hold it while writing to the Main=>Audio message queue.
 */
int io_audio_lock();
void io_audio_unlock();

/* Shovel Platform API, implemented by audio units.
 * sh_mr and sh_log are also available to the audio segment but are implemented by genioc.
 */
void sh_spcm(int chid,const float *pcm,int framec);

extern const char *io_audio_driver_name;

/* Input.
 *******************************************************************/

/* Drivers must tolerate redundant quit calls.
 */
void io_input_quit();

int io_input_init();

int io_input_update();

/* For video driver's use, to report keyboard state.
 */
void io_input_set_key(int keycode,int value);

/* Shovel Platform API, implemented by input units.
 */
int sh_in(int plrid);

extern const char *io_input_driver_name;

#endif
