# shovel

The simplest web+native game engine I can imagine.
This is heavily informed by a recent attempt called "rawwg", which I've abandoned.
Basic idea is the client code is Wasm, and it provides plain RGBX framebuffers and PCM to the platform.
Platform provides digested gamepad input, shovels those framebuffers and PCM out, and that's about it.

Actually, let's take a slightly different approach from usual.
Instead of an independant project that games plug in with, Shovel will be a template that you copy entirely.
That's a lesson learned from Upsy-Downsy, which uses Pebble, and is the only game using Pebble. Silly to have two repos involved.
I don't anticipate a lot of use of Shovel, maybe just the occasional size-coding challenge.

## TODO

 - [x] I want to deliver the ROM statically instead of depending on a function call. Confirm that that's possible.
 - - It is not possible. Options: Fetch function, Export pre-sized buffer, Bake in automatically, or do Nothing.
 - - I'm going with "Nothing". Games will be expected to bake their data into their wasm modules on their own.
 - - As I think about... Nothing is actually a pretty wise choice. Each segment only needs to embed the data it needs.
 - [x] Define API and data types.
 - [x] Tooling.
 - - Just the barest minimum to prove everything out.
 - [ ] Native runtime.
 - [ ] Web runtime.
