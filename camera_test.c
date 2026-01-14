/* Camera Orientation Test - Figure out the 90-degree rotation bug */

#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define PITCH 0
#define YAW   1
#define ROLL  2

typedef struct{float x,y,z;}V;

// Q3's AngleVectors (from q_math.c)
void q3_angle_vectors(float angles[3],V*fwd,V*right,V*up){
  float sy=sinf(angles[YAW]*M_PI/180);
  float cy=cosf(angles[YAW]*M_PI/180);
  float sp=sinf(angles[PITCH]*M_PI/180);
  float cp=cosf(angles[PITCH]*M_PI/180);
  float sr=sinf(angles[ROLL]*M_PI/180);
  float cr=cosf(angles[ROLL]*M_PI/180);

  if(fwd){
    fwd->x=cp*cy;
    fwd->y=cp*sy;
    fwd->z=-sp;
  }
  if(right){
    right->x=(-1*sr*sp*cy+-1*cr*-sy);
    right->y=(-1*sr*sp*sy+-1*cr*cy);
    right->z=-1*sr*cp;
  }
  if(up){
    up->x=(cr*sp*cy+-sr*-sy);
    up->y=(cr*sp*sy+-sr*cy);
    up->z=cr*cp;
  }
}

// My version (what's in q3.c)
void my_angle_vectors(float yaw,float pitch,V*fwd,V*right,V*up){
  float cy=cosf(yaw),sy=sinf(yaw),cp=cosf(pitch),sp=sinf(pitch);

  if(fwd){
    fwd->x=cy*cp;
    fwd->y=sy*cp;
    fwd->z=-sp;
  }
  if(right){
    right->x=-sy;
    right->y=cy;
    right->z=0;
  }
  if(up){
    // up = right × forward (cross product)
    up->x=right->y*fwd->z-right->z*fwd->y;
    up->y=right->z*fwd->x-right->x*fwd->z;
    up->z=right->x*fwd->y-right->y*fwd->x;
  }
}

int main(){
  printf("Camera Orientation Test\n\n");

  // Test yaw=0, pitch=0 (should face +X in Q3)
  float angles[3]={0,0,0};  // pitch,yaw,roll
  V q3_fwd,q3_right,q3_up;
  q3_angle_vectors(angles,&q3_fwd,&q3_right,&q3_up);

  printf("Q3 with yaw=0 degrees:\n");
  printf("  Forward: (%.3f, %.3f, %.3f)\n",q3_fwd.x,q3_fwd.y,q3_fwd.z);
  printf("  Right:   (%.3f, %.3f, %.3f)\n",q3_right.x,q3_right.y,q3_right.z);
  printf("  Up:      (%.3f, %.3f, %.3f)\n\n",q3_up.x,q3_up.y,q3_up.z);

  // My version with yaw=0 radians
  V my_fwd,my_right,my_up;
  my_angle_vectors(0,0,&my_fwd,&my_right,&my_up);

  printf("My code with yaw=0 radians:\n");
  printf("  Forward: (%.3f, %.3f, %.3f)\n",my_fwd.x,my_fwd.y,my_fwd.z);
  printf("  Right:   (%.3f, %.3f, %.3f)\n",my_right.x,my_right.y,my_right.z);
  printf("  Up:      (%.3f, %.3f, %.3f)\n\n",my_up.x,my_up.y,my_up.z);

  printf("Difference:\n");
  printf("  Forward: (%.3f, %.3f, %.3f)\n",
    q3_fwd.x-my_fwd.x,q3_fwd.y-my_fwd.y,q3_fwd.z-my_fwd.z);
  printf("  Right:   (%.3f, %.3f, %.3f)\n",
    q3_right.x-my_right.x,q3_right.y-my_right.y,q3_right.z-my_right.z);

  // Now test yaw=90 degrees
  angles[YAW]=90;
  q3_angle_vectors(angles,&q3_fwd,&q3_right,&q3_up);

  printf("\nQ3 with yaw=90 degrees (should face +Y):\n");
  printf("  Forward: (%.3f, %.3f, %.3f)\n",q3_fwd.x,q3_fwd.y,q3_fwd.z);
  printf("  Right:   (%.3f, %.3f, %.3f)\n",q3_right.x,q3_right.y,q3_right.z);

  my_angle_vectors(90*M_PI/180,0,&my_fwd,&my_right,&my_up);
  printf("\nMy code with yaw=90 degrees (radians):\n");
  printf("  Forward: (%.3f, %.3f, %.3f)\n",my_fwd.x,my_fwd.y,my_fwd.z);
  printf("  Right:   (%.3f, %.3f, %.3f)\n",my_right.x,my_right.y,my_right.z);

  return 0;
}
