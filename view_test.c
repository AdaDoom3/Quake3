#include <stdio.h>
#include <math.h>

int main(){
  // At spawn yaw=0, looking down corridor
  // Q3 vectors at yaw=0:
  float forward_x=1, forward_y=0, forward_z=0;
  float right_x=0, right_y=-1, right_z=0;
  float up_x=0, up_y=0, up_z=1;
  
  printf("Q3 camera basis at yaw=0:\n");
  printf("Forward: (%.0f, %.0f, %.0f) - points down +X corridor\n",forward_x,forward_y,forward_z);
  printf("Right: (%.0f, %.0f, %.0f) - points toward -Y (left wall to right wall)\n",right_x,right_y,right_z);
  printf("Up: (%.0f, %.0f, %.0f) - points toward +Z (floor to ceiling)\n",up_x,up_y,up_z);
  
  printf("\nOpenGL view matrix (column-major):\n");
  printf("Column 0 (right): [%.0f %.0f %.0f]\n",right_x,right_y,right_z);
  printf("Column 1 (up):    [%.0f %.0f %.0f]\n",up_x,up_y,up_z);
  printf("Column 2 (-fwd):  [%.0f %.0f %.0f]\n",-forward_x,-forward_y,-forward_z);
  
  printf("\nIf lights are at Y=-10 and Y=+10 (left/right walls):\n");
  printf("They should appear HORIZONTALLY on screen (left-right)\n");
  printf("Because they vary in the RIGHT direction\n");
  
  printf("\nIf lights are at Z=-10 and Z=+10 (floor/ceiling):\n");
  printf("They should appear VERTICALLY on screen (up-down)\n");
  printf("Because they vary in the UP direction\n");
  
  printf("\nSo if seeing vertical lights, they're varying in Z (up/down) in world\n");
  printf("NOT varying in Y (left/right)\n");
  
  return 0;
}
