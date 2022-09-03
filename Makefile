CC  = clang

LIBS = `pkg-config --libs --cflags sdl2` \
	   `pkg-config --libs --cflags vulkan`

LDFLAGS = -fuse-ld=lld -flto=thin
CFLAGS  = -march=native -O2 -g -std=c11 -fwrapv \
		  -fno-strict-aliasing \
		  -fno-delete-null-pointer-checks \
		  -funsigned-char

TARGET := target/main
ifeq ($(OS),Windows_NT)
	TARGET := $(addsuffix .exe,$(TARGET))
endif

SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:src/%.c=target/%.o)

ifeq ($(words $(SRCS)),0)
$(error there are no input files)
endif

all: $(TARGET)

$(TARGET): configure $(OBJS)
	$(CXX) $(LDFLAGS) $(LIBS) $(OBJS) -o $(TARGET)

configure:
	@mkdir -p target

run: $(TARGET)
	@./$(TARGET) --trace

clean:
	rm -rf target compile_commands.json

target/%.o: src/%.c
	$(CC) $(CFLAGS) -Iincludes -o $@ -c $<

.PHONY: all configure clean
