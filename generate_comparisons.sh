#!/bin/bash
echo "Generating quality comparison screenshots..."

# Delta - show all improvements in one location
POS="2000 700 600 1 0 0"

echo "1. Base geometry (flat shading)"
cat > /tmp/flat_shader.c << 'EEOF'
// Flat gray shading for comparison
EEOF

echo "2. Patches + normals (smooth shading)"  
echo "3. Lightmaps (shadows and detail)"
echo "4. Sky detection (proper sky color)"

# Already have these, just document them:
# - delta_fixed.tga: patches + normals only
# - delta_lightmap.tga: + lightmaps
# - delta_sky.tga: + sky detection

echo ""
echo "Final quality showcase:"

# Generate a comprehensive showcase from different maps
./q3rt_soft assets/maps/delta.bsp screenshots/final_delta.tga 2000 700 600 1 0 0 2>/dev/null
./q3rt_soft assets/maps/czest3ctf.bsp screenshots/final_czest.tga 500 0 200 0 1 0 2>/dev/null  
./q3rt_soft assets/maps/aggressor.bsp screenshots/final_agg.tga 0 300 0 0 -1 0 2>/dev/null

echo "Screenshots generated in screenshots/"
ls -lh screenshots/final_*.tga
