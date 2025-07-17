/* shovel.h
 * Platform for games, with client providing framebuffers and PCM directly.
 * A shovel game has two segments: Main and Audio.
 * They're linked together natively, but in web are two independent Wasm modules.
 *
 * "sh_" functions are provided by the platform for clients to call. Comments will note which segments are allowed to call it.
 * "shm_" functions are implemented by the client's main segment.
 * "sha_" functions are implemented by the client's audio segment.
 * Names are deliberately terse, because they must be repeated a few times in the JS runtime.
 *
 * ===== Shovel ROM Format =====
 * Binary file, big-endian integers.
 * Starts with TOC:
 *   4 Signature = "\0SVL"
 *   4 TOC length, ie offset to Main Segment. >=16. (includes Signature and itself, so <8 doesn't make sense).
 *   4 Main segment length.
 *   4 Audio segment length.
 *   ... Additional TOC space reserved for future use. Safe to ignore.
 * Followed by Main segment, a WebAssembly module.
 * Followed by Audio segment, a WebAssembly module.
 */
 
#ifndef SHOVEL_H
#define SHOVEL_H

/* Client must implement these.
 * 'shm_' functions go in the main segment, and 'sha_' function in the audio segment.
 ********************************************************************************/

/* Notify main segment of termination.
 * The platform doesn't guarantee this will be called every run, but will make an effort to.
 * (status) is zero for normal exit or >0 for errors, ie the process's exit status.
 * Typically noop. But you might want to log stats or flush a saved game out to storage.
 */
void shm_quit(int status);

/* Begin execution of the main segment.
 * If you return <0, the platform must terminate.
 * Both main and audio segments must initialize successfully before either will receive an update.
 */
int shm_init();

/* Update the main segment.
 * (elapsed) is the time in seconds since the last update, forced to a sensible range.
 * You may ignore (elapsed) and assume 60 Hz, but that won't always be exactly correct.
 * Typically, you will start with shovel_get_input() and end with shovel_set_framebuffer().
 */
void shm_update(double elapsed);

/* Begin execution of the audio segment.
 * If you return <0, the platform will terminate.
 * Both main and audio segments must initialize successfully before either will receive an update.
 * (rate) is the output rate in Hz. You must supply PCM at that rate, or fail.
 * (chanc) is the output channel count, typically 2. You're expected but not required to provide a stream for each channel.
 * You must call shovel_set_pcm_buffer() during sha_init() for each channel that you intend to produce.
 */
int sha_init(int rate,int chanc);

/* Advance synthesizer and produce so many frames of PCM.
 * Write your output to the buffers that you supplied to shovel_set_pcm_buffer(), during sha_init().
 * (framec) will never be more than the shortest buffer you supplied.
 */
void sha_update(int framec);

/* Platform API, for audio segment only.
 **************************************************************************/

/* Declare one channel of PCM.
 * This is only valid during sha_init().
 * (framec) must be at least 1, recommend at least 128.
 * Buffers for channels the platform doesn't have will be ignored.
 * You must hang on to these buffers, and repopulate them at each sha_update().
 */
void sh_spcm(int chid,const float *pcm,int framec);

/* Receive the next message from main segment.
 * This is a one-way line of communication: Audio segment can read but not write.
 * Returns zero if nothing is queued.
 */
int sh_mr(void *v,int a);

//TODO Should we provide some math functions? It would be trivial to export from the platform, and could spare a lot of trouble on the games' side.
 
/* Platform API, for both segments.
 ********************************************************************************/

/* Dump some text to the dev console.
 * Generally not visible to end users but not secret either.
 */
void sh_log(const char *msg);

/* Platform API, for main segment only.
 *******************************************************************************/

/* Queue a message for delivery to the audio segment.
 * Empty messages are an error.
 * Platform copies it immediately.
 */
int sh_ms(const void *v,int c);

/* Request platform to terminate at the next convenient moment.
 * Usually this does return, but it's allowed not to.
 */
void sh_term(int status);

/* Real time in seconds from some undefined epoch.
 * For game timing purposes, it's better to depend on (elapsed) passed to shm_update.
 */
double sh_now();

/* Access to persistent key=value store.
 * Lengths are required and strings generally will not be NUL-terminated.
 * "Store Get", "Store Set".
 */
int sh_sg(char *v,int va,const char *k,int kc);
int sh_ss(const char *k,int kc,const char *v,int vc);
 
#define SH_BTN_LEFT       0x01
#define SH_BTN_RIGHT      0x02
#define SH_BTN_UP         0x04
#define SH_BTN_DOWN       0x08
#define SH_BTN_SOUTH      0x10 /* Primary. */
#define SH_BTN_WEST       0x20 /* Secondary. */
#define SH_BTN_AUX1       0x40

/* Get the state of a virtual input device, keyed by player id.
 * Player zero is the aggregate of all devices, and most appropriate for single-player games.
 */
int sh_in(int plrid);

/* Framebuffer is packed 32-bit RGBX. Red in the first byte. The X component is ignored.
 * Pixels are packed LRTB, each row being (w*4) bytes long.
 * If you do not call this during shm_update(), whatever was displayed last frame remains.
 * If you call more than once, the last call wins.
 * You *must* keep (rgbx) in scope after shm_update completes. Platform won't read it until after.
 * By the same token, changes to your pixels after this call will be visible.
 */
void sh_fb(const void *rgbx,int w,int h);

#endif
