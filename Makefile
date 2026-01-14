# Quake3 Raytracing Engine Makefile
CC=gcc
CFLAGS=-std=c99 -O3 -Wall -Wextra -march=native -ffast-math
LIBS=-lglfw -lGL -lm
SRC=src/main.c
SRC_SS=src/screenshot.c
OUT=quake3rt
OUT_SS=screenshot_tool

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

$(OUT_SS): $(SRC_SS)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

screenshots: $(OUT_SS)
	xvfb-run -s "-screen 0 1024x768x24" ./$(OUT_SS)
	@echo "Converting to PNG..."
	@for i in 0 1 2 3; do \
		convert screenshot_$$i.ppm screenshots/shot_$$i.png 2>/dev/null || cp screenshot_$$i.ppm screenshots/shot_$$i.ppm; \
	done
	@echo "Screenshots saved in screenshots/ directory"

run: $(OUT)
	./$(OUT)

clean:
	rm -f $(OUT) $(OUT_SS) screenshot*.ppm

.PHONY: all run clean screenshots
