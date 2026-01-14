#!/bin/bash
# Run Q3 with simulated movement to test controls
export DISPLAY=:99
Xvfb :99 -screen 0 1920x1080x24 &
XVFB_PID=$!
sleep 1

./q3 assets/maps/dm4ish.bsp &
Q3_PID=$!

# Let it run for screenshots
sleep 3

kill $Q3_PID 2>/dev/null
kill $XVFB_PID 2>/dev/null
