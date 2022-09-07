CC = clang

LIBS = `pkg-config --libs --cflags sdl2` \
	   `pkg-config --libs --cflags vulkan`

LDFLAGS = -fuse-ld=lld -flto=thin
CFLAGS  = -std=c11 -fwrapv \
		  -fno-strict-aliasing \
		  -fno-delete-null-pointer-checks \
		  -funsigned-char

ifneq (, $(shell which lldb))
DEBUGGER := @lldb target/debug/main -o run -- --trace
else ifneq (, $(shell which gdb))
DEBUGGER := @gdb --args target/debug/main --trace
else
DEBUGGER := target/debug/main --trace
endif

SRCS = $(wildcard src/*.c)
DEB_OBJS = $(SRCS:src/%.c=target/debug/%.o)
REL_OBJS = $(SRCS:src/%.c=target/release/%.o)

ifeq ($(words $(SRCS)),0)
$(error there are no input files)
endif

all: CFLAGS+=-march=native -O2
all: target/release/main

debug: CFLAGS+=-g
debug: target/debug/main
	$(DEBUGGER)

release: CFLAGS+=-march=native -O2
release: target/release/main
	@target/release/main

configure_release:
	@mkdir -p target target/release

configure_debug:
	@mkdir -p target target/debug

clean:
	rm -rf target compile_commands.json

target/debug/main: configure_debug $(DEB_OBJS)
	$(CXX) $(LDFLAGS) $(LIBS) $(DEB_OBJS) -o $@

target/release/main: configure_release $(REL_OBJS)
	$(CXX) $(LDFLAGS) $(LIBS) $(REL_OBJS) -o $@

target/debug/%.o: src/%.c
	$(CC) $(CFLAGS) -Iincludes -o $@ -c $<

target/release/%.o: src/%.c
	$(CC) $(CFLAGS) -Iincludes -o $@ -c $<

.PHONY: all debug release configure_release configure_debug
