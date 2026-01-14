CC=gcc
CFLAGS=-O3 -march=native -std=c99 -D_DEFAULT_SOURCE -Wno-unused-result
LIBS=-lSDL2 -lvulkan -lm

all: q3vk shaders

shaders: q3.vert.glsl.spv q3.frag.glsl.spv

q3.vert.glsl.spv: q3.vert.glsl
	glslc -fshader-stage=vertex q3.vert.glsl -o q3.vert.glsl.spv

q3.frag.glsl.spv: q3.frag.glsl
	glslc -fshader-stage=fragment q3.frag.glsl -o q3.frag.glsl.spv

q3vk: q3vk.c
	$(CC) $(CFLAGS) -o q3vk q3vk.c $(LIBS)

clean:
	rm -f q3vk *.spv

run: all
	./q3vk

.PHONY: all clean run shaders
