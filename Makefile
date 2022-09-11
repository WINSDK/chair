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

SHADERS := $(wildcard src/*.vert) $(wildcard src/*.frag)
SHADERS := $(SHADERS:src/%=target/shaders/%.spv)

SAN_OBJS = $(SRCS:src/%.c=target/sanitize/%.o)
DEB_OBJS = $(SRCS:src/%.c=target/debug/%.o)
REL_OBJS = $(SRCS:src/%.c=target/release/%.o)

all: CFLAGS += -march=native -O2
all: target/release/main

sanitize: CFLAGS  += -g3 -Og -fsanitize=address -fno-omit-frame-pointer
sanitize: LDFLAGS += -fsanitize=address
sanitize: target/sanitize/main
	./target/sanitize/main --trace

debug: CFLAGS += -g3 -Og
debug: target/debug/main
	$(DEBUGGER)

release: CFLAGS += -march=native -O2
release: target/release/main
	./target/release/main

clean:
	rm -rf target compile_commands.json

target/shaders:
	@mkdir -p $@

target/sanitize:
	@mkdir -p $@

target/debug:
	@mkdir -p $@

target/release:
	@mkdir -p $@

target/sanitize/main: target/sanitize target/shaders $(SAN_OBJS) $(SHADERS)
	$(CC) $(LDFLAGS) $(LIBS) $(SAN_OBJS) -o $@

target/debug/main: target/debug target/shaders $(DEB_OBJS) $(SHADERS)
	$(CC) $(LDFLAGS) $(LIBS) $(DEB_OBJS) -o $@

target/release/main: target/release target/shaders $(REL_OBJS) $(SHADERS)
	$(CC) $(LDFLAGS) $(LIBS) $(REL_OBJS) -o $@
	strip --strip-all target/release/main

target/sanitize/%.o: src/%.c
	$(CC) $(CFLAGS) -Iincludes -o $@ -c $<

target/debug/%.o: src/%.c
	$(CC) $(CFLAGS) -Iincludes -o $@ -c $<

target/release/%.o: src/%.c
	$(CC) $(CFLAGS) -Iincludes -o $@ -c $<

target/shaders/%.vert.spv: src/%.vert
	glslc -O -o $@ $<

target/shaders/%.frag.spv: src/%.frag
	glslc -O -o $@ $<

.PHONY: all sanitize debug release clean
