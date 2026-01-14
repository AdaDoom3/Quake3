/* ═══════════════════════════════════════════════════════════════════════════
   ANIMATION SYSTEM - Header for Q3 Engine Integration

   Advanced features: Spring dynamics, cloth simulation, facial animation,
   muscle deformation, and real-time IK solvers.
   ═══════════════════════════════════════════════════════════════════════════*/

#ifndef ANIMATION_SYSTEM_H
#define ANIMATION_SYSTEM_H

#include <pthread.h>

// Core types
typedef struct{float x,y,z;}Vec3;
typedef struct{float x,y,z,w;}Quat;

// Animation state
typedef struct{
  Vec3*positions;
  Quat*rotations;
  float*weights;
  int count;
  float time;
}AnimationState;

// Skeletal hierarchy
typedef struct{
  int parent;
  char name[64];
  Vec3 bind_pos;
  Quat bind_rot;
  float length;
}Bone;

typedef struct{
  Bone bones[128];
  int bone_count;
  AnimationState current;
  AnimationState target;
  float blend_factor;
}Rig;

// IK solver configurations
typedef enum{
  IK_FABRIK,      // Forward And Backward Reaching
  IK_CCD,         // Cyclic Coordinate Descent
  IK_JACOBIAN,    // Pseudo-inverse Jacobian
  IK_TWO_BONE     // Analytic two-bone solver
}IKSolverType;

typedef struct{
  int chain_start;
  int chain_end;
  Vec3 target_pos;
  Vec3 pole_vector;
  IKSolverType solver;
  float weight;
  int iterations;
}IKConstraint;

// Spring dynamics for secondary motion
typedef struct{
  float stiffness;
  float damping;
  float mass;
  Vec3 rest_pos;
  Vec3 current_pos;
  Vec3 velocity;
}SpringBone;

// Muscle simulation
typedef struct{
  int bone_a,bone_b;
  float activation;
  float min_length,max_length;
  Vec3 insertion_a,insertion_b;
}Muscle;

// Facial animation blend shapes
typedef struct{
  Vec3*deltas;
  int vertex_count;
  float weight;
  char name[32];
}BlendShape;

// Main animation controller
typedef struct{
  Rig*rig;
  IKConstraint ik_constraints[16];
  int ik_count;
  SpringBone springs[64];
  int spring_count;
  Muscle muscles[32];
  int muscle_count;
  BlendShape blend_shapes[64];
  int blend_shape_count;
  pthread_mutex_t lock;
  int multi_threaded;
}AnimationController;

// API functions
AnimationController*anim_create(int num_bones);
void anim_destroy(AnimationController*ctrl);

// IK solvers
void anim_add_ik_constraint(AnimationController*ctrl,int start,int end,Vec3 target,IKSolverType type);
void anim_solve_ik(AnimationController*ctrl,float delta_time);

// Spring dynamics
void anim_add_spring_bone(AnimationController*ctrl,int bone_id,float stiffness,float damping);
void anim_update_springs(AnimationController*ctrl,float delta_time);

// Muscle system
void anim_add_muscle(AnimationController*ctrl,int bone_a,int bone_b,Vec3 ins_a,Vec3 ins_b);
void anim_activate_muscle(AnimationController*ctrl,int muscle_id,float activation);
void anim_update_muscles(AnimationController*ctrl);

// Blend shapes
void anim_add_blend_shape(AnimationController*ctrl,const char*name,Vec3*deltas,int count);
void anim_set_blend_shape_weight(AnimationController*ctrl,const char*name,float weight);

// Animation playback
void anim_play(AnimationController*ctrl,const char*anim_name,float blend_time);
void anim_update(AnimationController*ctrl,float delta_time);

// Procedural animation helpers
void anim_lookat(AnimationController*ctrl,int bone_id,Vec3 target,float weight);
void anim_aim(AnimationController*ctrl,int bone_id,Vec3 target,Vec3 up);
void anim_apply_noise(AnimationController*ctrl,int bone_id,float amplitude,float frequency);

// Hit reactions and physics integration
void anim_apply_impulse(AnimationController*ctrl,int bone_id,Vec3 impulse);
void anim_ragdoll_enable(AnimationController*ctrl,int enable);

#endif // ANIMATION_SYSTEM_H
