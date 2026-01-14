/* ═══════════════════════════════════════════════════════════════════════════
   FEATURE TEST HARNESS - Automated Screenshot Testing

   Tests specific features and captures screenshots for verification:
   - Test 1: Spawn point positioning
   - Test 2: Camera movement physics
   - Test 3: Animation (IK chain movement)
   - Test 4: Spring dynamics
   - Test 5: Weapon rendering (different scales)
   ═══════════════════════════════════════════════════════════════════════════*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct{
  int test_id;
  char name[64];
  char description[256];
  int start_frame;
  int end_frame;
  float cam_override[6]; // x,y,z,yaw,pitch,roll
  int use_cam_override;
}TestConfig;

static TestConfig tests[]={
  {
    .test_id=1,
    .name="spawn_point",
    .description="Verify player spawns at correct location with proper orientation",
    .start_frame=60,
    .end_frame=60,
    .use_cam_override=0
  },
  {
    .test_id=2,
    .name="forward_movement",
    .description="Test WASD physics - move forward 500 units",
    .start_frame=0,
    .end_frame=180,
    .use_cam_override=0
  },
  {
    .test_id=3,
    .name="camera_rotation",
    .description="Test mouse look - 360 degree rotation",
    .start_frame=0,
    .end_frame=240,
    .use_cam_override=0
  },
  {
    .test_id=4,
    .name="animation_ik",
    .description="Test IK system - move IK target in circle",
    .start_frame=0,
    .end_frame=300,
    .use_cam_override=1,
    .cam_override={100,0,0,0,0,0}
  },
  {
    .test_id=5,
    .name="spring_dynamics",
    .description="Test spring bones - oscillation and damping",
    .start_frame=0,
    .end_frame=200,
    .use_cam_override=1,
    .cam_override={50,50,50,-45,0,0}
  },
  {
    .test_id=6,
    .name="weapon_large_scale",
    .description="Test weapon rendering - increased scale to 0.1",
    .start_frame=60,
    .end_frame=60,
    .use_cam_override=0
  },
  {
    .test_id=7,
    .name="texture_variety",
    .description="Walk through map showing different textures",
    .start_frame=0,
    .end_frame=360,
    .use_cam_override=0
  },
  {
    .test_id=8,
    .name="lightmap_quality",
    .description="Static camera showing lightmap detail",
    .start_frame=120,
    .end_frame=120,
    .use_cam_override=1,
    .cam_override={-100,200,-150,135,15,0}
  }
};

void generate_test_script(const char*output_file){
  FILE*f=fopen(output_file,"w");
  if(!f){printf("Failed to create test script\n");return;}

  fprintf(f,"#!/bin/bash\n");
  fprintf(f,"# Auto-generated feature test script\n");
  fprintf(f,"# Tests specific engine features with screenshots\n\n");
  fprintf(f,"XVFB=\"xvfb-run -a -s '-screen 0 1920x1080x24'\"\n");
  fprintf(f,"ENGINE=\"./q3_integrated\"\n");
  fprintf(f,"MAP=\"assets/maps/dm4ish.bsp\"\n\n");

  for(int i=0;i<sizeof(tests)/sizeof(tests[0]);i++){
    TestConfig*t=&tests[i];
    fprintf(f,"echo \"=== Test %d: %s ===\"\n",t->test_id,t->name);
    fprintf(f,"echo \"%s\"\n",t->description);
    fprintf(f,"$XVFB $ENGINE $MAP --test %d --frames %d-%d\n",
      t->test_id,t->start_frame,t->end_frame);
    fprintf(f,"if [ -f test_%d_*.ppm ]; then\n",t->test_id);
    fprintf(f,"  for img in test_%d_*.ppm; do\n",t->test_id);
    fprintf(f,"    convert \"$img\" \"${img%%.ppm}.png\"\n");
    fprintf(f,"  done\n");
    fprintf(f,"  echo \"✓ Test %d complete\"\n",t->test_id);
    fprintf(f,"else\n");
    fprintf(f,"  echo \"✗ Test %d failed - no screenshots\"\n",t->test_id);
    fprintf(f,"fi\n");
    fprintf(f,"echo\n\n");
  }

  fprintf(f,"echo \"=== Test Summary ===\"\n");
  fprintf(f,"ls -lh test_*.png 2>/dev/null | wc -l\n");
  fprintf(f,"echo \"tests completed with screenshots\"\n");

  fclose(f);
  chmod(output_file,0755);
  printf("Generated test script: %s\n",output_file);
  printf("Run with: ./run_feature_tests.sh\n");
}

void generate_test_plan(){
  FILE*f=fopen("TEST_PLAN.md","w");
  if(!f)return;

  fprintf(f,"# Feature Test Plan - Q3 Integrated Engine\n\n");
  fprintf(f,"## Overview\n\n");
  fprintf(f,"Automated screenshot tests for verifying specific engine features.\n");
  fprintf(f,"Each test captures screenshots demonstrating a particular capability.\n\n");

  fprintf(f,"## Test Cases\n\n");

  for(int i=0;i<sizeof(tests)/sizeof(tests[0]);i++){
    TestConfig*t=&tests[i];
    fprintf(f,"### Test %d: %s\n\n",t->test_id,t->name);
    fprintf(f,"**Description**: %s\n\n",t->description);
    fprintf(f,"**Frames**: %d to %d\n\n",t->start_frame,t->end_frame);

    if(t->use_cam_override){
      fprintf(f,"**Camera Override**: Yes\n");
      fprintf(f,"- Position: (%.1f, %.1f, %.1f)\n",
        t->cam_override[0],t->cam_override[1],t->cam_override[2]);
      fprintf(f,"- Rotation: (%.1f°, %.1f°, %.1f°)\n\n",
        t->cam_override[3],t->cam_override[4],t->cam_override[5]);
    }else{
      fprintf(f,"**Camera Override**: No (uses spawn point)\n\n");
    }

    fprintf(f,"**Expected Output**: `test_%d_*.png`\n\n",t->test_id);

    fprintf(f,"**Verification Criteria**:\n");
    switch(t->test_id){
      case 1:
        fprintf(f,"- [ ] Player at spawn coordinates (64, 128, -164)\n");
        fprintf(f,"- [ ] Camera facing 0° (north)\n");
        fprintf(f,"- [ ] Proper player eye height visible\n");
        break;
      case 2:
        fprintf(f,"- [ ] Camera position changes over frames\n");
        fprintf(f,"- [ ] Movement appears smooth\n");
        fprintf(f,"- [ ] Physics speed realistic (300 units/sec)\n");
        break;
      case 3:
        fprintf(f,"- [ ] Camera rotates 360 degrees\n");
        fprintf(f,"- [ ] Full environment visible\n");
        fprintf(f,"- [ ] Rotation speed consistent\n");
        break;
      case 4:
        fprintf(f,"- [ ] IK chain bones visible\n");
        fprintf(f,"- [ ] Bones follow circular target path\n");
        fprintf(f,"- [ ] FABRIK solver working correctly\n");
        break;
      case 5:
        fprintf(f,"- [ ] Spring bones oscillating\n");
        fprintf(f,"- [ ] Damping visible (motion decays)\n");
        fprintf(f,"- [ ] No instability or explosions\n");
        break;
      case 6:
        fprintf(f,"- [ ] Weapon visible in lower-right\n");
        fprintf(f,"- [ ] Proper scale and positioning\n");
        fprintf(f,"- [ ] Weapon rendered on top (no occlusion)\n");
        break;
      case 7:
        fprintf(f,"- [ ] Multiple different textures\n");
        fprintf(f,"- [ ] All textures loaded correctly\n");
        fprintf(f,"- [ ] 0%% sky/error textures\n");
        break;
      case 8:
        fprintf(f,"- [ ] Lightmap shadows visible\n");
        fprintf(f,"- [ ] Smooth lighting gradients\n");
        fprintf(f,"- [ ] No banding or artifacts\n");
        break;
    }
    fprintf(f,"\n---\n\n");
  }

  fprintf(f,"## Running Tests\n\n");
  fprintf(f,"```bash\n");
  fprintf(f,"# Generate test script\n");
  fprintf(f,"gcc -o test_generator test_features.c && ./test_generator\n\n");
  fprintf(f,"# Run all tests\n");
  fprintf(f,"./run_feature_tests.sh\n\n");
  fprintf(f,"# View results\n");
  fprintf(f,"ls test_*.png\n");
  fprintf(f,"```\n\n");

  fprintf(f,"## Expected Results\n\n");
  fprintf(f,"- 8 test cases\n");
  fprintf(f,"- ~20-30 total screenshots\n");
  fprintf(f,"- Each test verifies specific feature\n");
  fprintf(f,"- All screenshots at 1920x1080 resolution\n\n");

  fprintf(f,"## Verification\n\n");
  fprintf(f,"After running tests, manually review screenshots:\n");
  fprintf(f,"1. Check each test_X_*.png file\n");
  fprintf(f,"2. Verify features according to criteria above\n");
  fprintf(f,"3. Document any failures in RESULTS.md\n");
  fprintf(f,"4. Fix issues and re-run specific tests\n\n");

  fprintf(f,"## Success Criteria\n\n");
  fprintf(f,"- ✅ All 8 tests generate screenshots\n");
  fprintf(f,"- ✅ Features visible and functioning\n");
  fprintf(f,"- ✅ No crashes or GL errors\n");
  fprintf(f,"- ✅ Performance stable (60 FPS)\n");

  fclose(f);
  printf("Generated test plan: TEST_PLAN.md\n");
}

int main(){
  printf("Feature Test Harness Generator\n");
  printf("===============================\n\n");

  generate_test_script("run_feature_tests.sh");
  printf("\n");
  generate_test_plan();

  printf("\nGenerated files:\n");
  printf("  - run_feature_tests.sh (executable script)\n");
  printf("  - TEST_PLAN.md (documentation)\n\n");

  printf("Next steps:\n");
  printf("  1. ./run_feature_tests.sh\n");
  printf("  2. Review test_*.png screenshots\n");
  printf("  3. Verify features per TEST_PLAN.md\n");

  return 0;
}
