/* ═══════════════════════════════════════════════════════════════════════════
   ANIMATION SYSTEM - Implementation
   ═══════════════════════════════════════════════════════════════════════════*/

#include "animation_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

AnimationController*anim_create(int num_bones){
  AnimationController*ctrl=calloc(1,sizeof(AnimationController));
  ctrl->rig=calloc(1,sizeof(Rig));
  ctrl->rig->bone_count=num_bones;
  ctrl->rig->current.positions=calloc(num_bones,sizeof(Vec3));
  ctrl->rig->current.rotations=calloc(num_bones,sizeof(Quat));
  ctrl->rig->current.weights=calloc(num_bones,sizeof(float));
  pthread_mutex_init(&ctrl->lock,NULL);

  // Initialize rotations to identity
  for(int i=0;i<num_bones;i++){
    ctrl->rig->current.rotations[i]=(Quat){0,0,0,1};
  }

  return ctrl;
}

void anim_destroy(AnimationController*ctrl){
  if(!ctrl)return;
  free(ctrl->rig->current.positions);
  free(ctrl->rig->current.rotations);
  free(ctrl->rig->current.weights);
  free(ctrl->rig);
  pthread_mutex_destroy(&ctrl->lock);
  free(ctrl);
}

void anim_add_ik_constraint(AnimationController*ctrl,int start,int end,Vec3 target,IKSolverType type){
  if(ctrl->ik_count>=16)return;
  ctrl->ik_constraints[ctrl->ik_count++]=(IKConstraint){
    .chain_start=start,.chain_end=end,.target_pos=target,
    .solver=type,.weight=1.0f,.iterations=10
  };
}

static float vec3_length(Vec3 v){
  return sqrtf(v.x*v.x+v.y*v.y+v.z*v.z);
}

static Vec3 vec3_normalize(Vec3 v){
  float len=vec3_length(v);
  return len>1e-6f?(Vec3){v.x/len,v.y/len,v.z/len}:(Vec3){0,1,0};
}

static Vec3 vec3_sub(Vec3 a,Vec3 b){
  return(Vec3){a.x-b.x,a.y-b.y,a.z-b.z};
}

static Vec3 vec3_add(Vec3 a,Vec3 b){
  return(Vec3){a.x+b.x,a.y+b.y,a.z+b.z};
}

static Vec3 vec3_scale(Vec3 v,float s){
  return(Vec3){v.x*s,v.y*s,v.z*s};
}

void anim_solve_ik(AnimationController*ctrl,float delta_time){
  for(int c=0;c<ctrl->ik_count;c++){
    IKConstraint*ik=&ctrl->ik_constraints[c];

    if(ik->solver==IK_FABRIK){
      // FABRIK implementation
      int chain_len=ik->chain_end-ik->chain_start+1;
      if(chain_len<2)continue;

      Vec3 base=ctrl->rig->current.positions[ik->chain_start];
      float total_len=0;

      for(int i=ik->chain_start;i<ik->chain_end;i++){
        total_len+=ctrl->rig->bones[i].length;
      }

      float dist=vec3_length(vec3_sub(ik->target_pos,base));

      if(dist>total_len+1e-6f){
        // Unreachable - extend maximally
        Vec3 dir=vec3_normalize(vec3_sub(ik->target_pos,base));
        for(int i=ik->chain_start+1;i<=ik->chain_end;i++){
          float len=ctrl->rig->bones[i-1].length;
          ctrl->rig->current.positions[i]=vec3_add(
            ctrl->rig->current.positions[i-1],
            vec3_scale(dir,len)
          );
        }
      }else{
        // FABRIK iterations
        for(int iter=0;iter<ik->iterations;iter++){
          // Forward pass
          ctrl->rig->current.positions[ik->chain_end]=ik->target_pos;
          for(int i=ik->chain_end-1;i>=ik->chain_start;i--){
            Vec3 dir=vec3_normalize(vec3_sub(
              ctrl->rig->current.positions[i],
              ctrl->rig->current.positions[i+1]
            ));
            ctrl->rig->current.positions[i]=vec3_add(
              ctrl->rig->current.positions[i+1],
              vec3_scale(dir,ctrl->rig->bones[i].length)
            );
          }

          // Backward pass
          ctrl->rig->current.positions[ik->chain_start]=base;
          for(int i=ik->chain_start+1;i<=ik->chain_end;i++){
            Vec3 dir=vec3_normalize(vec3_sub(
              ctrl->rig->current.positions[i],
              ctrl->rig->current.positions[i-1]
            ));
            ctrl->rig->current.positions[i]=vec3_add(
              ctrl->rig->current.positions[i-1],
              vec3_scale(dir,ctrl->rig->bones[i-1].length)
            );
          }
        }
      }
    }
  }

  // Reset count for next frame
  ctrl->ik_count=0;
}

void anim_add_spring_bone(AnimationController*ctrl,int bone_id,float stiffness,float damping){
  if(ctrl->spring_count>=64)return;
  ctrl->springs[ctrl->spring_count++]=(SpringBone){
    .stiffness=stiffness,.damping=damping,.mass=1.0f,
    .rest_pos={0,0,0},.current_pos={0,0,0},.velocity={0,0,0}
  };
}

void anim_update_springs(AnimationController*ctrl,float delta_time){
  for(int i=0;i<ctrl->spring_count;i++){
    SpringBone*s=&ctrl->springs[i];

    // Hooke's law: F = -k * x
    Vec3 displacement=vec3_sub(s->current_pos,s->rest_pos);
    Vec3 spring_force=vec3_scale(displacement,-s->stiffness);

    // Damping: F = -c * v
    Vec3 damping_force=vec3_scale(s->velocity,-s->damping);

    // Total force
    Vec3 total_force=vec3_add(spring_force,damping_force);

    // a = F/m
    Vec3 accel=vec3_scale(total_force,1.0f/s->mass);

    // Semi-implicit Euler
    s->velocity=vec3_add(s->velocity,vec3_scale(accel,delta_time));
    s->current_pos=vec3_add(s->current_pos,vec3_scale(s->velocity,delta_time));
  }
}

void anim_add_muscle(AnimationController*ctrl,int bone_a,int bone_b,Vec3 ins_a,Vec3 ins_b){
  if(ctrl->muscle_count>=32)return;
  ctrl->muscles[ctrl->muscle_count++]=(Muscle){
    .bone_a=bone_a,.bone_b=bone_b,.activation=0,
    .min_length=0.5f,.max_length=2.0f,
    .insertion_a=ins_a,.insertion_b=ins_b
  };
}

void anim_activate_muscle(AnimationController*ctrl,int muscle_id,float activation){
  if(muscle_id>=0&&muscle_id<ctrl->muscle_count){
    ctrl->muscles[muscle_id].activation=activation;
  }
}

void anim_update_muscles(AnimationController*ctrl){
  // Simple muscle simulation - pull bones together based on activation
  for(int i=0;i<ctrl->muscle_count;i++){
    Muscle*m=&ctrl->muscles[i];
    if(m->activation==0)continue;

    Vec3 pa=ctrl->rig->current.positions[m->bone_a];
    Vec3 pb=ctrl->rig->current.positions[m->bone_b];

    Vec3 dir=vec3_normalize(vec3_sub(pb,pa));
    float target_len=m->min_length+(m->max_length-m->min_length)*(1-m->activation);

    // Pull towards target length
    ctrl->rig->current.positions[m->bone_b]=vec3_add(pa,vec3_scale(dir,target_len));
  }
}

void anim_add_blend_shape(AnimationController*ctrl,const char*name,Vec3*deltas,int count){
  if(ctrl->blend_shape_count>=64)return;
  BlendShape*bs=&ctrl->blend_shapes[ctrl->blend_shape_count++];
  strncpy(bs->name,name,31);
  bs->vertex_count=count;
  bs->deltas=malloc(count*sizeof(Vec3));
  memcpy(bs->deltas,deltas,count*sizeof(Vec3));
  bs->weight=0;
}

void anim_set_blend_shape_weight(AnimationController*ctrl,const char*name,float weight){
  for(int i=0;i<ctrl->blend_shape_count;i++){
    if(strcmp(ctrl->blend_shapes[i].name,name)==0){
      ctrl->blend_shapes[i].weight=weight;
      return;
    }
  }
}

void anim_play(AnimationController*ctrl,const char*anim_name,float blend_time){
  // Stub for animation playback
}

void anim_update(AnimationController*ctrl,float delta_time){
  pthread_mutex_lock(&ctrl->lock);
  anim_solve_ik(ctrl,delta_time);
  anim_update_springs(ctrl,delta_time);
  anim_update_muscles(ctrl);
  pthread_mutex_unlock(&ctrl->lock);
}

void anim_lookat(AnimationController*ctrl,int bone_id,Vec3 target,float weight){
  // Stub for look-at
}

void anim_aim(AnimationController*ctrl,int bone_id,Vec3 target,Vec3 up){
  // Stub for aiming
}

void anim_apply_noise(AnimationController*ctrl,int bone_id,float amplitude,float frequency){
  // Stub for noise
}

void anim_apply_impulse(AnimationController*ctrl,int bone_id,Vec3 impulse){
  // Stub for impulse
}

void anim_ragdoll_enable(AnimationController*ctrl,int enable){
  // Stub for ragdoll
}
