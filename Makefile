CC = clang

LDFLAGS = -flto=thin
CFLAGS  = -std=c11 -fwrapv \
		  -fno-strict-aliasing \
		  -fno-delete-null-pointer-checks \
		  -funsigned-char \
		  -Wall \
		  -Wfloat-equal \
		  -Wstring-compare \
		  -Wuninitialized \

CFLAGS += `pkg-config --cflags vulkan` \
		  `pkg-config --cflags sdl2`

LDFLAGS += `pkg-config --libs vulkan` \
		   `pkg-config --libs sdl2`

ifneq (, $(shell which lldb 2> /dev/null))
DEBUGGER := lldb ./target/debug/main -o run -- --trace
else ifneq (, $(shell which gdb 2> /dev/null))
DEBUGGER := gdb --args ./target/debug/main --trace
else
DEBUGGER := ./target/debug/main --trace
endif

SRCS = $(wildcard src/*.c)

SHADERS := $(wildcard src/*.vert) $(wildcard src/*.frag)
SHADERS := $(SHADERS:src/%=target/%.spv)

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

target/sanitize:
	@mkdir -p $@

target/debug:
	@mkdir -p $@

target/release:
	@mkdir -p $@

target/sanitize/main: target/sanitize $(SAN_OBJS) $(SHADERS)
	$(CC) $(LDFLAGS) $(SAN_OBJS) -o $@

target/debug/main: target/debug $(DEB_OBJS) $(SHADERS)
	$(CC) $(LDFLAGS) $(DEB_OBJS) -o $@

target/release/main: target/release $(REL_OBJS) $(SHADERS)
	$(CC) $(LDFLAGS) $(REL_OBJS) -o $@
	strip --strip-all $@

target/sanitize/%.o: src/%.c
	$(CC) $(CFLAGS) -Iincludes -o $@ -c $<

target/debug/%.o: src/%.c
	$(CC) $(CFLAGS) -Iincludes -o $@ -c $<

target/release/%.o: src/%.c
	$(CC) $(CFLAGS) -Iincludes -o $@ -c $<

target/%.vert.spv: src/%.vert
	glslangValidator -V -S vert -o $@ $<

target/%.frag.spv: src/%.frag
	glslangValidator -V -S frag -o $@ $<

.PHONY: all sanitize debug release clean
