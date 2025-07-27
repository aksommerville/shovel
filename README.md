# shovel

The simplest web+native game engine I can imagine.
This is heavily informed by a recent attempt called "rawwg", which I've abandoned.
Basic idea is the client code is Wasm, and it provides plain RGBX framebuffers and PCM to the platform.
Platform provides digested gamepad input, shovels those framebuffers and PCM out, and that's about it.

Actually, let's take a slightly different approach from usual.
Instead of an independant project that games plug in with, Shovel will be a template that you copy entirely.
That's a lesson learned from Upsy-Downsy, which uses Pebble, and is the only game using Pebble. Silly to have two repos involved.
I don't anticipate a lot of use of Shovel, maybe just the occasional size-coding challenge.

Since we're going bare-minimum as a general principle, there will be a single-driver model for native builds.
You must enable exactly one unit each for audio, video, and input, and those units export the same genericish symbols.

An observer might be confused as to why we're not using a 3rd-party Javascript minifier.
There's no doubt that doing so would lead to smaller distributables.
But I want the project as self-contained as possible, so no 3rd-party code in the main build pipeline unless it's absolutely necessary.

## TODO

 - [ ] Native: General input mapping.
 - [ ] Native: Suppress screensaver on gamepad input.
 - [ ] Remaining linux drivers: drm,alsafd,asound,bcm. Can and should we do an X11 no-GX driver?
 - [ ] MacOS drivers.
 - [ ] MS Windows drivers.
 - [ ] SDL drivers?
 - [ ] genioc: argv
 - [ ] Record and playback native sessions. Playback in headless with video and audio capture.
 - [ ] Optional client-side utilities.
 - - [ ] malloc
 - - [ ] math
 - - [ ] PRNG
 - - [ ] Slightly nicer synthesizer.
 - - [ ] Slightly nicer renderer.
 - - [ ] PNG-like image decoder. Can we use regular PNG? Would need zlib.
 - [x] xegl: Bounds go weird when you resize.
