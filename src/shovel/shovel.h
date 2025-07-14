/* shovel.h
 * Platform for games, with client providing framebuffers and PCM directly.
 * A shovel game has two segments: Main and Audio.
 * They're linked together natively, but in web are two independent Wasm modules.
 *
 * ===== Shovel ROM Format =====
 * Binary file, big-endian integers.
 * Starts with TOC:
 *   4 Signature = "\0SVL"
 *   4 TOC length, ie offset to Metadata. >=20. (includes Signature and itself, so <8 doesn't make sense).
 *   4 Metadata length.
 *   4 Main segment length.
 *   4 Audio segment length.
 *   ... Additional TOC space reserved for future use. Safe to ignore.
 * Followed by Metadata, zero or more of:
 *   1 kc
 *   1 vc
 *   ... k
 *   ... v
 *   (k,v) are ASCII strings.
 *   See etc/doc/metadata.txt for defined keys. TODO
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

/* Main segment defines this, but our tooling takes care of it, game doesn't need to.
 * This will not exist in web builds; there it has a privileged position in the ROM instead.
 */
extern const unsigned char shm_metadata[];
extern const int shm_metadata_length;

/* Platform API, for audio segment only.
 **************************************************************************/

/* Declare one channel of PCM.
 * This is only valid during sha_init().
 * (framec) must be at least 1, recommend at least 128.
 * Buffers for channels the platform doesn't have will be ignored.
 * You must hang on to these buffers, and repopulate them at each sha_update().
 */
void shovel_set_pcm_buffer(int chid,const float *pcm,int framec);

//TODO Should we provide some math functions? It would be trivial to export from the platform, and could spare a lot of trouble on the games' side.
 
/* Platform API, for both segments.
 ********************************************************************************/

/* Dump some text to the dev console.
 * Generally not visible to end users but not secret either.
 */
void shovel_log(const char *msg);

/* Platform API, for main segment only.
 *******************************************************************************/

/* Request platform to terminate at the next convenient moment.
 * Usually this does return, but it's allowed not to.
 */
void shovel_terminate(int status);

/* Real time in seconds from some undefined epoch.
 * For game timing purposes, it's better to depend on (elapsed) passed to shm_update.
 */
double shovel_now_real();

/* Access to persistent key=value store.
 * Lengths are required and strings generally will not be NUL-terminated.
 */
int shovel_store_get(char *v,int va,const char *k,int kc);
int shovel_store_set(const char *k,int kc,const char *v,int vc);

/* Queue messages for the other segment, or pull from your queue.
 * shovel_msg_receive() will return zero if nothing more is pending.
 * Messages longer than 256 bytes will be rejected.
 * The platform has an internal limit of at least 16 messages and will reject new ones if full.
 * If you supply a buffer too short at receive, we'll copy what fits, return the full length, and pop it from the queue.
 * The remainder is gone forever.
 * Typically messages only go in one direction, main=>audio, and are commands to play a song or sound.
 * You're allowed to send from audio to main too, eg for reporting playhead position.
 */
int shovel_msg_send(const void *v,int c);
int shovel_msg_receive(void *v,int a);
 
#define SHOVEL_BTN_LEFT       0x01
#define SHOVEL_BTN_RIGHT      0x02
#define SHOVEL_BTN_UP         0x04
#define SHOVEL_BTN_DOWN       0x08
#define SHOVEL_BTN_SOUTH      0x10 /* Primary. */
#define SHOVEL_BTN_WEST       0x20 /* Secondary. */
#define SHOVEL_BTN_AUX1       0x40

/* Get the state of a virtual input device, keyed by player id.
 * Player zero is the aggregate of all devices, and most appropriate for single-player games.
 */
int shovel_get_input(int plrid);

/* Framebuffer is packed 32-bit RGBX. Red in the first byte. The X component is ignored.
 * Pixels are packed LRTB, each row being (w*4) bytes long.
 * If you do not call this during shm_update(), whatever was displayed last frame remains.
 * If you call more than once, the last call wins.
 * You *must* keep (rgbx) in scope after shm_update completes. Platform won't read it until after.
 * By the same token, changes to your pixels after this call will be visible.
 */
void shovel_set_framebuffer(const void *rgbx,int w,int h);

#endif
