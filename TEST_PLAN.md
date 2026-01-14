# Feature Test Plan - Q3 Integrated Engine

## Overview

Automated screenshot tests for verifying specific engine features.
Each test captures screenshots demonstrating a particular capability.

## Test Cases

### Test 1: spawn_point

**Description**: Verify player spawns at correct location with proper orientation

**Frames**: 60 to 60

**Camera Override**: No (uses spawn point)

**Expected Output**: `test_1_*.png`

**Verification Criteria**:
- [ ] Player at spawn coordinates (64, 128, -164)
- [ ] Camera facing 0° (north)
- [ ] Proper player eye height visible

---

### Test 2: forward_movement

**Description**: Test WASD physics - move forward 500 units

**Frames**: 0 to 180

**Camera Override**: No (uses spawn point)

**Expected Output**: `test_2_*.png`

**Verification Criteria**:
- [ ] Camera position changes over frames
- [ ] Movement appears smooth
- [ ] Physics speed realistic (300 units/sec)

---

### Test 3: camera_rotation

**Description**: Test mouse look - 360 degree rotation

**Frames**: 0 to 240

**Camera Override**: No (uses spawn point)

**Expected Output**: `test_3_*.png`

**Verification Criteria**:
- [ ] Camera rotates 360 degrees
- [ ] Full environment visible
- [ ] Rotation speed consistent

---

### Test 4: animation_ik

**Description**: Test IK system - move IK target in circle

**Frames**: 0 to 300

**Camera Override**: Yes
- Position: (100.0, 0.0, 0.0)
- Rotation: (0.0°, 0.0°, 0.0°)

**Expected Output**: `test_4_*.png`

**Verification Criteria**:
- [ ] IK chain bones visible
- [ ] Bones follow circular target path
- [ ] FABRIK solver working correctly

---

### Test 5: spring_dynamics

**Description**: Test spring bones - oscillation and damping

**Frames**: 0 to 200

**Camera Override**: Yes
- Position: (50.0, 50.0, 50.0)
- Rotation: (-45.0°, 0.0°, 0.0°)

**Expected Output**: `test_5_*.png`

**Verification Criteria**:
- [ ] Spring bones oscillating
- [ ] Damping visible (motion decays)
- [ ] No instability or explosions

---

### Test 6: weapon_large_scale

**Description**: Test weapon rendering - increased scale to 0.1

**Frames**: 60 to 60

**Camera Override**: No (uses spawn point)

**Expected Output**: `test_6_*.png`

**Verification Criteria**:
- [ ] Weapon visible in lower-right
- [ ] Proper scale and positioning
- [ ] Weapon rendered on top (no occlusion)

---

### Test 7: texture_variety

**Description**: Walk through map showing different textures

**Frames**: 0 to 360

**Camera Override**: No (uses spawn point)

**Expected Output**: `test_7_*.png`

**Verification Criteria**:
- [ ] Multiple different textures
- [ ] All textures loaded correctly
- [ ] 0% sky/error textures

---

### Test 8: lightmap_quality

**Description**: Static camera showing lightmap detail

**Frames**: 120 to 120

**Camera Override**: Yes
- Position: (-100.0, 200.0, -150.0)
- Rotation: (135.0°, 15.0°, 0.0°)

**Expected Output**: `test_8_*.png`

**Verification Criteria**:
- [ ] Lightmap shadows visible
- [ ] Smooth lighting gradients
- [ ] No banding or artifacts

---

## Running Tests

```bash
# Generate test script
gcc -o test_generator test_features.c && ./test_generator

# Run all tests
./run_feature_tests.sh

# View results
ls test_*.png
```

## Expected Results

- 8 test cases
- ~20-30 total screenshots
- Each test verifies specific feature
- All screenshots at 1920x1080 resolution

## Verification

After running tests, manually review screenshots:
1. Check each test_X_*.png file
2. Verify features according to criteria above
3. Document any failures in RESULTS.md
4. Fix issues and re-run specific tests

## Success Criteria

- ✅ All 8 tests generate screenshots
- ✅ Features visible and functioning
- ✅ No crashes or GL errors
- ✅ Performance stable (60 FPS)
