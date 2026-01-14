#!/bin/bash
glslc -fshader-stage=vertex q3.vert.glsl -o q3.vert.glsl.spv
glslc -fshader-stage=fragment q3.frag.glsl -o q3.frag.glsl.spv
gcc -o q3vk q3vk.c -lSDL2 -lvulkan -lm -O3 -march=native -std=c99 -D_DEFAULT_SOURCE -Wno-unused-result
