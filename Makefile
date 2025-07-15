all:
.SILENT:
.SECONDARY:
PRECMD=echo "  $@" ; mkdir -p $(@D) ;
OPTDEF=$(foreach U,$($1_OPT_ENABLE),-DUSE_$U=1)

ifneq (,$(strip $(filter clean,$(MAKECMDGOALS))))
clean:;rm -rf mid out
else

PROJNAME:=shoveldemo

SRCFILES:=$(shell find src -type f)

# Host-specific configuration lives in a separate file which one-time-pulls from the repo.
# Things like toggling cross-compile targets, or setting particular warning options, that's all in config.mk.
# You shouldn't need to edit this Makefile, and certainly not for build-host-specific concerns.
include local/config.mk

#----------------------------------------------------------------
# Build the tool. This is a distinct target and is required.

tool_CFILES:=$(filter src/tool/%.c $(addprefix src/opt/,$(addsuffix /%.c,$(tool_OPT_ENABLE))),$(SRCFILES))
tool_OFILES:=$(patsubst src/%.c,mid/tool/%.o,$(tool_CFILES))
-include $(tool_OFILES:.o=.d)
mid/tool/%.o:src/%.c;$(PRECMD) $(tool_CC) -o$@ $<
tool_EXE:=out/tool
all:$(tool_EXE)
$(tool_EXE):$(tool_OFILES);$(PRECMD) $(tool_LD) -o$@ $^ $(tool_LDPOST)

#---------------------------------------------------------------
# Divide the targets into "WEBLIKE" and "NATIVELIKE".
# WEBLIKE targets build two independant Wasm modules, then combine them into a ROM file.
# NATIVELIKE targets are generic, nothing peculiar to Shovel about them.
# For now, the only WEBLIKE target is "web".

WEBLIKE_TARGETS:=$(filter web,$(TARGETS))
NATIVELIKE_TARGETS:=$(filter-out $(WEBLIKE_TARGETS),$(TARGETS))

#-------------------------------------------------------------
# Web targets.

WEBRT_SRCFILES:=$(filter src/www/%,$(SRCFILES))

define WEBLIKE_RULES
  $1_MAIN_CFILES:=$$(filter src/main/%.c $$(addprefix src/opt/,$$(addsuffix /%.c,$$($1_MAIN_OPT_ENABLE))),$(SRCFILES))
  $1_AUDIO_CFILES:=$$(filter src/audio/%.c $$(addprefix src/opt/,$$(addsuffix /%.c,$$($1_AUDIO_OPT_ENABLE))),$(SRCFILES))
  $1_MAIN_OFILES:=$$(patsubst src/%.c,mid/$1-main/%.o,$$($1_MAIN_CFILES))
  $1_AUDIO_OFILES:=$$(patsubst src/%.c,mid/$1-audio/%.o,$$($1_AUDIO_CFILES))
  -include $$($1_MAIN_OFILES:.o=.d)
  -include $$($1_AUDIO_OFILES:.o=.d)
  mid/$1-main/%.o:src/%.c;$$(PRECMD) $$($1_MAIN_CC) -o$$@ $$<
  mid/$1-audio/%.o:src/%.c;$$(PRECMD) $$($1_AUDIO_CC) -o$$@ $$<
  $1_MAIN_WASM:=mid/$1-main/main.wasm
  $1_AUDIO_WASM:=mid/$1-audio/audio.wasm
  $$($1_MAIN_WASM):$$($1_MAIN_OFILES);$$(PRECMD) $$($1_MAIN_LD) -o$$@ $$^ $$($1_MAIN_LDPOST)
  $$($1_AUDIO_WASM):$$($1_AUDIO_OFILES);$$(PRECMD) $$($1_AUDIO_LD) -o$$@ $$^ $$($1_AUDIO_LDPOST)
  $1_ROM:=out/$1/game.rom
  $1_HTML:=out/$1/index.html
  $1_ZIP:=out/$1/$(PROJNAME).zip
  all:$$($1_ROM) $$($1_HTML) $$($1_ZIP)
  $$($1_ROM):src/metadata $$($1_MAIN_WASM) $$($1_AUDIO_WASM) $(tool_EXE);$$(PRECMD) $(tool_EXE) pack -o$$@ src/metadata $$($1_MAIN_WASM) $$($1_AUDIO_WASM)
  $$($1_HTML):$(WEBRT_SRCFILES) $(tool_EXE);$$(PRECMD) $(tool_EXE) html -o$$@ src/www/index.html
  $$($1_ZIP):$$($1_ROM) $$($1_HTML);$$(PRECMD) rm -f $$@ ; zip -j $$@ $$^
endef

$(foreach T,$(WEBLIKE_TARGETS),$(eval $(call WEBLIKE_RULES,$T)))

ifneq (,$(strip $(filter web,$(WEBLIKE_TARGETS))))
  #TODO This will need to be HTTPS. So generate a self-signed certificate, and write a node server.
  serve:$(web_ROM) $(web_HTML);http-server -c-1 out/web
endif

#------------------------------------------------------------------
# Native targets.

define NATIVELIKE_RULES
  $1_CFILES:=$$(filter src/main/%.c src/audio/%.c $$(addprefix src/opt/,$$(addsuffix /%.c,$$($1_OPT_ENABLE))),$(SRCFILES))
  $1_OFILES:=$$(patsubst src/%.c,mid/$1/%.o,$$($1_CFILES))
  -include $$($1_OFILES:.o=.d)
  mid/$1/%.o:src/%.c;$$(PRECMD) $$($1_CC) -o$$@ $$<
  $$($1_EXE):$$($1_OFILES);$$(PRECMD) $$($1_LD) -o$$@ $$^ $$($1_LDPOST)
  all:$$($1_EXE)
endef

$(foreach T,$(NATIVELIKE_TARGETS),$(eval $(call NATIVELIKE_RULES,$T)))

ifneq (,$(strip $(filter $(NATIVE_TARGET),$(NATIVELIKE_TARGETS))))
  run:$($(NATIVE_TARGET)_EXE);$<
endif

endif
