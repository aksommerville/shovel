# local/config.mk
# A template is in the shared repo, but after cloning this will not commit by default.
# Modify to suit this particular build host.

# TARGETS: Which hosts do we build for? web linux macos mswin generic, or make one up and define it below.
TARGETS:=web linux generic

# NATIVE_TARGET: The one involved in 'make run', possibly other privileged roles.
NATIVE_TARGET:=linux

# "tool" target is required, and must not be declared as a TARGET.
tool_OPT_ENABLE:=fs serial png midi
tool_CC:=gcc -c -MMD -O3 -Werror -Wimplicit -Wno-parentheses -Isrc $(call OPTDEF,tool)
tool_LD:=gcc
tool_LDPOST:=-lm -lz

# "web" target is a little weird because we actually build two modules independently.
web_MAIN_OPT_ENABLE:=web r1b
web_AUDIO_OPT_ENABLE:=web synmin
web_MAIN_CC:=clang -c -MMD -O3 --target=wasm32 -nostdlib -Werror -Wno-comment -Wno-parentheses -Isrc -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header $(call OPTDEF,web_MAIN)
web_AUDIO_CC:=clang -c -MMD -O3 --target=wasm32 -nostdlib -Werror -Wno-comment -Wno-parentheses -Isrc -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header $(call OPTDEF,web_AUDIO)
web_MAIN_LD:=wasm-ld --no-entry -z stack-size=4194304 --no-gc-sections --import-undefined --export-table --export=shm_quit --export=shm_init --export=shm_update
web_AUDIO_LD:=wasm-ld --no-entry -z stack-size=4194304 --no-gc-sections --import-undefined --export-table --export=sha_init --export=sha_update

# "generic" to build a headless native version of the app, maybe for automated testing?
generic_OPT_ENABLE:=genioc io fs a_dummy v_dummy i_dummy r1b synmin
generic_CC:=gcc -c -MMD -O3 -Werror -Wimplicit -Wno-parentheses -Isrc $(call OPTDEF,generic)
generic_LD:=gcc
generic_LDPOST:=-lm
generic_EXE:=out/generic/$(PROJNAME)

# "linux": Highly configurable. You must ensure that CC, LD, and LDPOST agree with OPT_ENABLE. See the relevant opt headers for more.
linux_OPT_ENABLE:=genioc io fs a_pulse v_xegl i_evdev xinerama r1b synmin
linux_CC:=gcc -c -MMD -Os -Werror -Wimplicit -Wno-parentheses -Isrc $(call OPTDEF,linux)
linux_LD:=gcc -s -Os
linux_LDPOST:=-lm -lpulse-simple -lX11 -lXinerama -lGL -lEGL
linux_EXE:=out/linux/$(PROJNAME)

# "macos" has additional packaging rules managed by Makefile. We're only concerned here with the executable, which works just like other hosts.
macos_OPT_ENABLE:=macos io fs r1b synmin
macos_CC:=gcc -c -MMD -O3 -Werror -Wimplicit -Wno-parentheses -Isrc $(call OPTDEF,macos)
macos_LD:=gcc
macos_LDPOST:=-lm
macos_EXE:=out/macos/$(PROJNAME).app/Contents/MacOS/$(PROJNAME)

# "mswin" isn't currently implemented and probably never will be.
mswin_OPT_ENABLE:=mswin genioc io fs r1b synmin
mswin_CC:=gcc -c -MMD -O3 -Werror -Wimplicit -Wno-parentheses -Isrc $(call OPTDEF,mswin)
mswin_LD:=gcc
mswin_LDPOST:=-lm
mswin_EXE:=out/mswin/$(PROJNAME).exe
