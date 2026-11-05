SRC = main.c
CC = zig cc

TARGETS = arm64 arm

CFLAGS_arm64 = -target aarch64-linux
CFLAGS_arm = -target arm-linux -mthumb

CFLAGS += -Oz -flto -s -static
CFLAGS += -std=gnu2y -Wl,--entry=__start
CFLAGS += -fno-unwind-tables -fmerge-all-constants
CFLAGS += -Wl,--gc-sections,-z,norelro
CFLAGS += -Wno-int-conversion -Wno-unknown-attributes

all: $(TARGETS)

$(TARGETS): $(SRC)
	@mkdir -p out
	$(CC) $(CFLAGS) $(CFLAGS_$@) $(SRC) -o out/toolkit-$@
	@wc -c out/toolkit-$@
	@if command -v sstrip >/dev/null 2>&1; then sstrip -z out/toolkit-$@; fi
	@wc -c out/toolkit-$@
	@file out/toolkit-$@

clean:
	rm -rf out/

.PHONY: all clean
