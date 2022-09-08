CC = clang

LIBS = `pkg-config --libs --cflags sdl2` \
	   `pkg-config --libs --cflags vulkan`

LDFLAGS = -fuse-ld=lld -flto=thin
CFLAGS  = -std=c11 -fwrapv \
		  -fno-strict-aliasing \
		  -fno-delete-null-pointer-checks \
		  -funsigned-char \
		  -Wall \
		  -Wfloat-equal \
		  -Wstring-compare \
		  -Wuninitialized

ifneq (, $(shell which lldb))
DEBUGGER := lldb ./target/debug/main -o run -- --trace
else ifneq (, $(shell which gdb))
DEBUGGER := gdb --args ./target/debug/main --trace
else
DEBUGGER := ./target/debug/main --trace
endif

SRCS = $(wildcard src/*.c)
SAN_OBJS = $(SRCS:src/%.c=target/sanitize/%.o)
DEB_OBJS = $(SRCS:src/%.c=target/debug/%.o)
REL_OBJS = $(SRCS:src/%.c=target/release/%.o)

ifeq ($(words $(SRCS)),0)
$(error there are no input files)
endif

all: CFLAGS += -march=native -O2
all: target/release/main

sanitize: CFLAGS  += -g3 -Og -fsanitize=address -fno-omit-frame-pointer
sanitize: LDFLAGS += -fsanitize=address
sanitize: target/sanitize/main target/sanitize/dlclose.so
	LD_PRELOAD="./target/sanitize/dlclose.so" ./target/sanitize/main --trace

debug: CFLAGS += -g3 -Og
debug: target/debug/main
	$(DEBUGGER)

release: CFLAGS += -march=native -O2
release: target/release/main
	./target/release/main

configure_sanitize:
	@mkdir -p target target/sanitize

configure_debug:
	@mkdir -p target target/debug

configure_release:
	@mkdir -p target target/release

clean:
	rm -rf target compile_commands.json

target/sanitize/main: configure_sanitize $(SAN_OBJS)
	$(CC) $(LDFLAGS) $(LIBS) $(SAN_OBJS) -o $@

target/debug/main: configure_debug $(DEB_OBJS)
	$(CC) $(LDFLAGS) $(LIBS) $(DEB_OBJS) -o $@

target/release/main: configure_release $(REL_OBJS)
	$(CC) $(LDFLAGS) $(LIBS) $(REL_OBJS) -o $@
	strip --strip-all target/release/main

target/sanitize/dlclose.so: src/dlclose.c
	$(CC) --shared -o $@ $<

target/sanitize/%.o: src/%.c
	$(CC) $(CFLAGS) -Iincludes -o $@ -c $<

target/debug/%.o: src/%.c
	$(CC) $(CFLAGS) -Iincludes -o $@ -c $<

target/release/%.o: src/%.c
	$(CC) $(CFLAGS) -Iincludes -o $@ -c $<

.PHONY: all sanitize debug release configure_sanitize configure_debug configure_release
