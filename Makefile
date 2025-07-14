all:
.SILENT:
.SECONDARY:
PRECMD=echo "  $@" ; mkdir -p $(@D) ;

clean:;rm -rf mid out

#XXX quick-n-dirty build for web, to test static rom delivery
web_CC:=clang -c -MMD -O3 --target=wasm32 -nostdlib -Werror -Wno-comment -Wno-parentheses -Isrc/shovel -Isrc/demo/src/game -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header
web_LD:=wasm-ld --no-entry -z stack-size=4194304 --no-gc-sections --import-undefined --export-table \
  --export=shm_quit --export=shm_init --export=shm_update --export=my_rom --export=my_rom_size
native_CC:=gcc -c -MMD -O3 -Isrc -Isrc/shovel -Werror -Wimplicit
native_LD:=gcc
native_LDPOST:=
SRCFILES:=$(shell find src -type f)
native_CFILES:=$(filter src/native/%.c src/opt/%.c src/demo/%.c,$(SRCFILES))
native_OFILES:=$(patsubst src/%.c,mid/native/%.o,$(native_CFILES))
-include $(native_OFILES:.o=.d)
web_CFILES:=$(filter src/demo/src/game/%.c,$(SRCFILES))
web_OFILES:=$(patsubst src/%.c,mid/web/%.o,$(web_CFILES))
-include $(web_OFILES:.o=.d)
mid/web/%.o:src/%.c;$(PRECMD) $(web_CC) -o$@ $<
mid/native/%.o:src/%.c;$(PRECMD) $(native_CC) -o$@ $<
native_EXE:=out/native
all:$(native_EXE)
$(native_EXE):$(native_OFILES);$(PRECMD) $(native_LD) -o$@ $^ $(native_LDPOST)
web_MAIN_WASM:=out/main.wasm
all:$(web_MAIN_WASM)
$(web_MAIN_WASM):$(web_OFILES);$(PRECMD) $(web_LD) -o$@ $^

serve:$(web_MAIN_WASM);http-server -c-1 out
