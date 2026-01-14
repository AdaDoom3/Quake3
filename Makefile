# Quake3 Raytracing Engine Makefile
CC=gcc
CFLAGS=-std=c99 -O3 -Wall -Wextra -march=native -ffast-math
LIBS=-lglfw -lGL -lm

# Source files
SRC=src/main.c
SRC_SS=src/screenshot.c
SRC_TEST=src/test_suite.c
SRC_BENCH=src/perf_bench.c

# Output binaries
OUT=quake3rt
OUT_SS=screenshot_tool
OUT_TEST=test_suite
OUT_BENCH=perf_bench

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

$(OUT_SS): $(SRC_SS)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

$(OUT_TEST): $(SRC_TEST)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

$(OUT_BENCH): $(SRC_BENCH)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

screenshots: $(OUT_SS)
	@mkdir -p screenshots
	xvfb-run -s "-screen 0 1024x768x24" ./$(OUT_SS)
	@echo "Converting to PNG..."
	@for i in 0 1 2 3; do \
		convert screenshot_$$i.ppm screenshots/shot_$$i.png 2>/dev/null || cp screenshot_$$i.ppm screenshots/shot_$$i.ppm; \
	done
	@echo "Screenshots saved in screenshots/ directory"

test: $(OUT_TEST)
	@mkdir -p tests
	@echo "Running comprehensive test suite..."
	xvfb-run -a -s "-screen 0 1024x768x24" ./$(OUT_TEST)
	@echo "Test results saved in tests/ directory"

benchmark: $(OUT_BENCH)
	@echo "Running performance benchmarks..."
	xvfb-run -a -s "-screen 0 1920x1080x24" ./$(OUT_BENCH) | tee benchmark_results.txt
	@echo "Results saved to benchmark_results.txt"

run: $(OUT)
	./$(OUT)

clean:
	rm -f $(OUT) $(OUT_SS) $(OUT_TEST) $(OUT_BENCH)
	rm -f screenshot*.ppm benchmark_results.txt

clean-all: clean
	rm -rf screenshots/ tests/

.PHONY: all run clean clean-all screenshots test benchmark
