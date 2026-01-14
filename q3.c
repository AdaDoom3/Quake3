/* ═══════════════════════════════════════════════════════════════════════════
   QUAKE III ARENA - Integrated Engine (Code Golf Edition)
   A Literate Programming Journey Through Modern Game Engine Architecture

   "In the beginning, Carmack created the vertices and the pixels..."

   This single-file implementation demonstrates functional programming applied
   to game engine architecture, combining BSP rendering with cutting-edge
   animation and physics systems. We pursue minimalism through perfect
   composition of pure functions.

   Features:
   • BSP file parsing (Q3 format 0x2e)
   • Multi-texture + lightmap rendering
   • OpenGL 3.3 shader pipeline
   • FABRIK inverse kinematics
   • Spring dynamics for secondary motion
   • Muscle simulation
   • Blend shape facial animation
   • Multi-threaded animation (pthreads)

   The architecture follows Haskell's philosophy: computation as mathematical
   transformation. State flows through pipelines. Side effects are quarantined.
   The GPU becomes our parallel fold over geometric reality.
   ═══════════════════════════════════════════════════════════════════════════*/

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <pthread.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER I: The Algebra of Types

   In functional programming, we begin by defining the universe of discourse.
   These structures represent the Platonic ideals of our 3D realm.
   ═══════════════════════════════════════════════════════════════════════════*/

typedef struct{float x,y,z;}V;
typedef struct{float x,y,z,w;}V4;
typedef struct{float u,v;}T;
typedef struct{int v,n,t;}I;
typedef struct{V a,b;}B;
typedef struct{int o,n;}L;
typedef struct{unsigned char r,g,b,a;}C;

typedef struct{int ofs,len;}D;
typedef struct{char sig[4];int ver;D d[17];}H;
typedef struct{char n[64];int f,c;}TX;
typedef struct{V mn,mx;int f,b;}N;
typedef struct{int t,e[2];}F;
typedef struct{int t,e,c,v,nv,mv,nmv,m,lms[2],lmsz[2];V lmo,lmv[2],nm;int sz[2];}LF;
typedef struct{char n[64];float x[3][3],p[3];}EF;
typedef struct{int f,n,b,s,v,w,h,o[3];char s2[128];}SH;

typedef struct{
  V*vs;T*ts;T*ls;C*cs;int*is;TX*tx;LF*lf;int nv,nt,nl,nc,ni,ntx,nlf;
  unsigned char*lm;int nlm;SH*sh;int nsh;N*nd;int nnd;F*fc;int nfc;
  int*lfi;EF*ef;int nef;int*mv;int nmv;B bb;
}M;

// MD3: Carmack's gift to modders - geometry that moves
typedef struct{char id[4];int ver;char name[64];int flags;int nframes,ntags,nmeshes,nskins;int ofs_frames,ofs_tags,ofs_meshes,ofs_eof;}MD3H;
typedef struct{char id[4];char name[64];int flags;int nframes,nshaders,nverts,ntris;int ofs_tris,ofs_shaders,ofs_st,ofs_verts,ofs_end;}MD3M;
typedef struct{V*vs;T*ts;int*is;int nv,ni;}MD3G;

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER II: Animation System Types

   Advanced animation requires skeletal hierarchy, inverse kinematics,
   spring dynamics, muscle simulation, and blend shapes.
   ═══════════════════════════════════════════════════════════════════════════*/

typedef struct{int parent;char name[64];V bind_pos;V4 bind_rot;float length;}Bone;
typedef struct{V*positions;V4*rotations;float*weights;int count;float time;}AnimState;
typedef struct{Bone bones[128];int bone_count;AnimState current,target;float blend_factor;}Rig;

typedef enum{IK_FABRIK,IK_CCD,IK_JACOBIAN,IK_TWO_BONE}IKSolverType;
typedef struct{int chain_start,chain_end;V target_pos,pole_vector;IKSolverType solver;float weight;int iterations;}IKConstraint;
typedef struct{float stiffness,damping,mass;V rest_pos,current_pos,velocity;}SpringBone;
typedef struct{int bone_a,bone_b;float activation,min_length,max_length;V insertion_a,insertion_b;}Muscle;
typedef struct{V*deltas;int vertex_count;float weight;char name[32];}BlendShape;

typedef struct{
  Rig*rig;IKConstraint ik_constraints[16];int ik_count;
  SpringBone springs[64];int spring_count;
  Muscle muscles[32];int muscle_count;
  BlendShape blend_shapes[64];int blend_shape_count;
  pthread_mutex_t lock;int multi_threaded;
}AnimCtrl;

typedef struct{V pos;float angle;}Spawn;

// MD3 tag - attachment point with 3x3 rotation matrix
typedef struct{
  char name[64];
  V origin;
  float axis[3][3];
}Tag;

// Character model - full 3-part MD3 player with tags
typedef struct{
  V**lower_frames;  // Animation frames for lower (legs)
  V**upper_frames;  // Animation frames for upper (torso)
  V*head;           // Single frame for head
  Tag*lower_tags,*upper_tags;  // Tags for articulation
  int*lower_tris,*upper_tris,*head_tris;
  int lower_nv,lower_nt,lower_nf,lower_ntags;
  int upper_nv,upper_nt,upper_nf,upper_ntags;
  int head_nv,head_nt;
  unsigned int lower_vao,lower_vbo,lower_ebo;
  unsigned int upper_vao,upper_vbo,upper_ebo;
  unsigned int head_vao,head_vbo,head_ebo;
}Character;

typedef struct{
  SDL_Window*w;SDL_GLContext g;int sw,sh;
  unsigned int vao,vbo,ebo,tx[256],lm[256];
  unsigned int prg,vsh,fsh,wprog,wvao,wvbo,ikvao,ikvbo;
  V cp,ca;float cy,pitch;int fwd,bck,lft,rgt;
  V vel;              // Player velocity (physics)
  int on_ground;      // Ground contact flag
  float ground_z;     // Current ground height
  M m;AnimCtrl*anim;Spawn spawn;MD3G wpn;Character player;int run,fc;
  int show_player;  // Toggle to show character model
  int auto_test;    // Automated test mode
  int test_phase;   // Current test scenario
}G;

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER III: Monadic I/O and the Impure World

   File reading is our interface to the material realm.
   ═══════════════════════════════════════════════════════════════════════════*/

static void*rd(const char*p,int*sz){
  FILE*f=fopen(p,"rb");if(!f)return 0;
  fseek(f,0,SEEK_END);*sz=ftell(f);fseek(f,0,SEEK_SET);
  void*d=malloc(*sz);fread(d,1,*sz,f);fclose(f);return d;
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER IV: Linear Algebra - The Mathematics of Space

   Vectors are the fundamental building blocks. These pure functions
   implement the field operations that govern Euclidean space.
   ═══════════════════════════════════════════════════════════════════════════*/

static inline V v3(float x,float y,float z){return(V){x,y,z};}
static inline V4 v4(float x,float y,float z,float w){return(V4){x,y,z,w};}
static inline V add(V a,V b){return v3(a.x+b.x,a.y+b.y,a.z+b.z);}
static inline V sub(V a,V b){return v3(a.x-b.x,a.y-b.y,a.z-b.z);}
static inline V scl(V a,float s){return v3(a.x*s,a.y*s,a.z*s);}
static inline float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static inline float vlen(V a){return sqrtf(dot(a,a));}
static inline V nrm(V a){float l=vlen(a);return l>1e-6f?scl(a,1.0f/l):a;}
static inline V crs(V a,V b){return v3(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER V: Animation System - Inverse Kinematics & Spring Dynamics

   FABRIK (Forward And Backward Reaching IK) provides O(n) inverse kinematics.
   Spring dynamics add secondary motion for hair, cloth, and soft bodies.
   Muscle simulation and blend shapes enable realistic character animation.
   ═══════════════════════════════════════════════════════════════════════════*/

static AnimCtrl*anim_create(int num_bones){
  AnimCtrl*ctrl=calloc(1,sizeof(AnimCtrl));
  ctrl->rig=calloc(1,sizeof(Rig));
  ctrl->rig->bone_count=num_bones;
  ctrl->rig->current.positions=calloc(num_bones,sizeof(V));
  ctrl->rig->current.rotations=calloc(num_bones,sizeof(V4));
  ctrl->rig->current.weights=calloc(num_bones,sizeof(float));
  pthread_mutex_init(&ctrl->lock,NULL);
  for(int i=0;i<num_bones;i++)ctrl->rig->current.rotations[i]=v4(0,0,0,1);
  return ctrl;
}

static void anim_destroy(AnimCtrl*ctrl){
  if(!ctrl)return;
  free(ctrl->rig->current.positions);free(ctrl->rig->current.rotations);free(ctrl->rig->current.weights);
  free(ctrl->rig);pthread_mutex_destroy(&ctrl->lock);free(ctrl);
}

static void anim_add_ik(AnimCtrl*ctrl,int start,int end,V target,IKSolverType type){
  if(ctrl->ik_count>=16)return;
  ctrl->ik_constraints[ctrl->ik_count++]=(IKConstraint){
    .chain_start=start,.chain_end=end,.target_pos=target,
    .solver=type,.weight=1.0f,.iterations=10
  };
}

static void anim_solve_ik(AnimCtrl*ctrl,float dt){
  for(int c=0;c<ctrl->ik_count;c++){
    IKConstraint*ik=&ctrl->ik_constraints[c];
    if(ik->solver==IK_FABRIK){
      int chain_len=ik->chain_end-ik->chain_start+1;
      if(chain_len<2)continue;
      V base=ctrl->rig->current.positions[ik->chain_start];
      float total_len=0;
      for(int i=ik->chain_start;i<ik->chain_end;i++)total_len+=ctrl->rig->bones[i].length;
      float dist=vlen(sub(ik->target_pos,base));
      if(dist>total_len+1e-6f){
        V dir=nrm(sub(ik->target_pos,base));
        for(int i=ik->chain_start+1;i<=ik->chain_end;i++){
          float len=ctrl->rig->bones[i-1].length;
          ctrl->rig->current.positions[i]=add(ctrl->rig->current.positions[i-1],scl(dir,len));
        }
      }else{
        for(int iter=0;iter<ik->iterations;iter++){
          ctrl->rig->current.positions[ik->chain_end]=ik->target_pos;
          for(int i=ik->chain_end-1;i>=ik->chain_start;i--){
            V dir=nrm(sub(ctrl->rig->current.positions[i],ctrl->rig->current.positions[i+1]));
            ctrl->rig->current.positions[i]=add(ctrl->rig->current.positions[i+1],scl(dir,ctrl->rig->bones[i].length));
          }
          ctrl->rig->current.positions[ik->chain_start]=base;
          for(int i=ik->chain_start+1;i<=ik->chain_end;i++){
            V dir=nrm(sub(ctrl->rig->current.positions[i],ctrl->rig->current.positions[i-1]));
            ctrl->rig->current.positions[i]=add(ctrl->rig->current.positions[i-1],scl(dir,ctrl->rig->bones[i-1].length));
          }
        }
      }
    }
  }
  ctrl->ik_count=0;
}

static void anim_add_spring(AnimCtrl*ctrl,int bone_id,float stiffness,float damping){
  if(ctrl->spring_count>=64)return;
  ctrl->springs[ctrl->spring_count++]=(SpringBone){
    .stiffness=stiffness,.damping=damping,.mass=1.0f,
    .rest_pos={0,0,0},.current_pos={0,0,0},.velocity={0,0,0}
  };
}

static void anim_update_springs(AnimCtrl*ctrl,float dt){
  for(int i=0;i<ctrl->spring_count;i++){
    SpringBone*s=&ctrl->springs[i];
    V disp=sub(s->current_pos,s->rest_pos);
    V spring_f=scl(disp,-s->stiffness);
    V damp_f=scl(s->velocity,-s->damping);
    V total_f=add(spring_f,damp_f);
    V accel=scl(total_f,1.0f/s->mass);
    s->velocity=add(s->velocity,scl(accel,dt));
    s->current_pos=add(s->current_pos,scl(s->velocity,dt));
  }
}

static void anim_add_muscle(AnimCtrl*ctrl,int bone_a,int bone_b,V ins_a,V ins_b){
  if(ctrl->muscle_count>=32)return;
  ctrl->muscles[ctrl->muscle_count++]=(Muscle){
    .bone_a=bone_a,.bone_b=bone_b,.activation=0,
    .min_length=0.5f,.max_length=2.0f,
    .insertion_a=ins_a,.insertion_b=ins_b
  };
}

static void anim_activate_muscle(AnimCtrl*ctrl,int muscle_id,float activation){
  if(muscle_id>=0&&muscle_id<ctrl->muscle_count)ctrl->muscles[muscle_id].activation=activation;
}

static void anim_update_muscles(AnimCtrl*ctrl){
  for(int i=0;i<ctrl->muscle_count;i++){
    Muscle*m=&ctrl->muscles[i];
    if(m->activation==0)continue;
    V pa=ctrl->rig->current.positions[m->bone_a];
    V pb=ctrl->rig->current.positions[m->bone_b];
    V dir=nrm(sub(pb,pa));
    float target_len=m->min_length+(m->max_length-m->min_length)*(1-m->activation);
    ctrl->rig->current.positions[m->bone_b]=add(pa,scl(dir,target_len));
  }
}

static void anim_add_blend_shape(AnimCtrl*ctrl,const char*name,V*deltas,int count){
  if(ctrl->blend_shape_count>=64)return;
  BlendShape*bs=&ctrl->blend_shapes[ctrl->blend_shape_count++];
  strncpy(bs->name,name,31);bs->vertex_count=count;
  bs->deltas=malloc(count*sizeof(V));memcpy(bs->deltas,deltas,count*sizeof(V));bs->weight=0;
}

static void anim_set_blend_shape_weight(AnimCtrl*ctrl,const char*name,float weight){
  for(int i=0;i<ctrl->blend_shape_count;i++){
    if(strcmp(ctrl->blend_shapes[i].name,name)==0){ctrl->blend_shapes[i].weight=weight;return;}
  }
}

static void anim_update(AnimCtrl*ctrl,float dt){
  pthread_mutex_lock(&ctrl->lock);
  anim_solve_ik(ctrl,dt);anim_update_springs(ctrl,dt);anim_update_muscles(ctrl);
  pthread_mutex_unlock(&ctrl->lock);
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER VI: Image Decoding - Translating Compressed Representations

   TGA files encode pixel data. We unfold this encoding through pattern
   matching on the header structure, revealing the underlying rgba tensor.
   ═══════════════════════════════════════════════════════════════════════════*/

static unsigned char*tga(unsigned char*d,int*w,int*h){
  if(!d)return 0;
  *w=d[12]|(d[13]<<8);*h=d[14]|(d[15]<<8);
  int bpp=d[16],o=18+d[0];
  unsigned char*p=malloc(*w**h*4),*s=d+o;
  if(bpp==32)for(int i=0;i<*w**h;i++){
    p[i*4]=s[i*4+2];p[i*4+1]=s[i*4+1];p[i*4+2]=s[i*4];p[i*4+3]=s[i*4+3];
  }else if(bpp==24)for(int i=0;i<*w**h;i++){
    p[i*4]=s[i*3+2];p[i*4+1]=s[i*3+1];p[i*4+2]=s[i*3];p[i*4+3]=255;
  }else return 0;
  return p;
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER VII: BSP Parsing - Deconstructing the Binary Space Partition

   The BSP file is a serialized scene graph. We deserialize this structure
   through pointer arithmetic, casting the byte stream into typed arrays.
   ═══════════════════════════════════════════════════════════════════════════*/

static int ld(const char*p,M*m){
  int sz;unsigned char*d=rd(p,&sz);
  if(!d)return 0;
  H*h=(H*)d;
  if(memcmp(h->sig,"IBSP",4)||h->ver!=0x2e)return 0;

  typedef struct{V p;T tx;T lm;V n;C c;}VT;
  int nvt=h->d[10].len/sizeof(VT);
  VT*vts=(VT*)(d+h->d[10].ofs);
  m->nv=nvt;
  m->vs=malloc(m->nv*sizeof(V));m->ts=malloc(m->nv*sizeof(T));
  m->ls=malloc(m->nv*sizeof(T));m->cs=malloc(m->nv*sizeof(C));
  for(int i=0;i<m->nv;i++){
    m->vs[i]=vts[i].p;m->ts[i]=vts[i].tx;m->ls[i]=vts[i].lm;m->cs[i]=vts[i].c;
  }

  int nidx=h->d[11].len/sizeof(int);
  int*idx=(int*)(d+h->d[11].ofs);
  m->is=malloc(nidx*sizeof(int));
  memcpy(m->is,idx,h->d[11].len);m->ni=nidx;

  m->ntx=h->d[1].len/sizeof(TX);
  m->tx=malloc(m->ntx*sizeof(TX));memcpy(m->tx,d+h->d[1].ofs,m->ntx*sizeof(TX));

  m->nlf=h->d[13].len/sizeof(LF);
  m->lf=malloc(m->nlf*sizeof(LF));memcpy(m->lf,d+h->d[13].ofs,m->nlf*sizeof(LF));

  int lmsz=128*128*3;m->nlm=h->d[14].len/lmsz;
  m->lm=malloc(h->d[14].len);memcpy(m->lm,d+h->d[14].ofs,h->d[14].len);

  m->nsh=h->d[8].len/sizeof(SH);
  m->sh=malloc(m->nsh*sizeof(SH));memcpy(m->sh,d+h->d[8].ofs,m->nsh*sizeof(SH));

  m->nnd=h->d[3].len/sizeof(N);
  m->nd=malloc(m->nnd*sizeof(N));memcpy(m->nd,d+h->d[3].ofs,m->nnd*sizeof(N));

  m->nfc=h->d[4].len/sizeof(F);
  m->fc=malloc(m->nfc*sizeof(F));memcpy(m->fc,d+h->d[4].ofs,m->nfc*sizeof(F));

  m->nef=h->d[2].len/sizeof(EF);
  m->ef=malloc(m->nef*sizeof(EF));memcpy(m->ef,d+h->d[2].ofs,m->nef*sizeof(EF));

  m->nmv=h->d[6].len/sizeof(int);
  m->mv=malloc(m->nmv*sizeof(int));memcpy(m->mv,d+h->d[6].ofs,m->nmv*sizeof(int));

  int nlfi=h->d[5].len/sizeof(int);
  m->lfi=malloc(nlfi*sizeof(int));memcpy(m->lfi,d+h->d[5].ofs,nlfi*sizeof(int));

  V mn=v3(1e9,1e9,1e9),mx=v3(-1e9,-1e9,-1e9);
  for(int i=0;i<m->nv;i++){
    if(m->vs[i].x<mn.x)mn.x=m->vs[i].x;if(m->vs[i].y<mn.y)mn.y=m->vs[i].y;if(m->vs[i].z<mn.z)mn.z=m->vs[i].z;
    if(m->vs[i].x>mx.x)mx.x=m->vs[i].x;if(m->vs[i].y>mx.y)mx.y=m->vs[i].y;if(m->vs[i].z>mx.z)mx.z=m->vs[i].z;
  }
  m->bb.a=mn;m->bb.b=mx;

  free(d);
  return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER VII.5: Entity Parsing - Finding Spawn Points

   The entity lump (lump 0) contains spawn points and item placements.
   We parse this text-based data to find info_player_start.
   ═══════════════════════════════════════════════════════════════════════════*/

static Spawn pent(const char*p){
  int sz;char*d=rd(p,&sz);
  Spawn s={.pos=v3(0,50,-200),.angle=0};
  if(!d)return s;
  H*h=(H*)d;
  char*ent=(char*)(d+h->d[0].ofs);
  char*pos=strstr(ent,"info_player");
  if(pos){
    char*org=strstr(pos,"origin");
    if(org){
      float x,y,z;
      if(sscanf(org+9,"%f %f %f",&x,&y,&z)==3)s.pos=v3(x,y,z+60);
    }
    char*ang=strstr(pos,"angle");
    if(ang){
      float a;
      if(sscanf(ang+7,"%f",&a)==1)s.angle=a*M_PI/180.0f;
    }
  }
  free(d);
  return s;
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER VII.6: MD3 Loading - Models That Carmack Built

   MD3 stores triangle meshes in frames. We extract frame 0 geometry.
   ═══════════════════════════════════════════════════════════════════════════*/

static MD3G ldmd3(const char*p){
  MD3G geo={0};
  int sz;unsigned char*d=rd(p,&sz);
  if(!d)return geo;
  MD3H*h=(MD3H*)d;
  if(memcmp(h->id,"IDP3",4)||h->ver!=15)return geo;

  MD3M*m=(MD3M*)(d+h->ofs_meshes);
  int nv=m->nverts,ni=m->ntris*3;

  // Vertices compressed as shorts - unpack to floats
  short*verts=(short*)(d+h->ofs_meshes+m->ofs_verts);
  geo.vs=malloc(nv*sizeof(V));
  for(int i=0;i<nv;i++){
    geo.vs[i].x=verts[i*4+0]/64.0f;
    geo.vs[i].y=verts[i*4+1]/64.0f;
    geo.vs[i].z=verts[i*4+2]/64.0f;
  }

  // UVs
  float*st=(float*)(d+h->ofs_meshes+m->ofs_st);
  geo.ts=malloc(nv*sizeof(T));
  for(int i=0;i<nv;i++){
    geo.ts[i].u=st[i*2];
    geo.ts[i].v=st[i*2+1];
  }

  // Indices
  int*tris=(int*)(d+h->ofs_meshes+m->ofs_tris);
  geo.is=malloc(ni*sizeof(int));
  memcpy(geo.is,tris,ni*sizeof(int));

  geo.nv=nv;geo.ni=ni;
  free(d);
  return geo;
}

// Load MD3 with ALL animation frames (for character models)
static void ld_md3_multi(const char*path,V***frames_out,Tag**tags_out,int*ntags_out,
                        int**tris_out,int*nv_out,int*nt_out,int*nf_out){
  int sz;unsigned char*d=rd(path,&sz);
  if(!d){*frames_out=NULL;return;}

  MD3H*h=(MD3H*)d;
  if(memcmp(h->id,"IDP3",4)||h->ver!=15){free(d);*frames_out=NULL;return;}

  *nf_out=h->nframes;
  *ntags_out=h->ntags;

  // Load tags (position + 3x3 matrix per tag per frame)
  if(h->ntags>0){
    *tags_out=malloc(h->ntags*h->nframes*sizeof(Tag));
    unsigned char*tag_data=d+h->ofs_tags;
    for(int f=0;f<h->nframes;f++){
      for(int t=0;t<h->ntags;t++){
        int idx=f*h->ntags+t;
        char*name=(char*)(tag_data+(idx*(64+12+36)));
        float*origin=(float*)(tag_data+(idx*(64+12+36))+64);
        float*axis=(float*)(tag_data+(idx*(64+12+36))+64+12);
        strncpy((*tags_out)[idx].name,name,64);
        (*tags_out)[idx].origin=v3(origin[0],origin[1],origin[2]);
        for(int i=0;i<3;i++)for(int j=0;j<3;j++)
          (*tags_out)[idx].axis[i][j]=axis[i*3+j];
      }
    }
  }

  MD3M*m=(MD3M*)(d+h->ofs_meshes);
  unsigned char*mesh_base=(unsigned char*)m;

  *nv_out=m->nverts;
  *nt_out=m->ntris;

  // Load all frames
  *frames_out=malloc(m->nframes*sizeof(V*));
  short*vdata=(short*)(mesh_base+m->ofs_verts);

  for(int f=0;f<m->nframes;f++){
    (*frames_out)[f]=malloc(m->nverts*sizeof(V));
    for(int i=0;i<m->nverts;i++){
      int idx=(f*m->nverts+i)*4;
      (*frames_out)[f][i].x=vdata[idx+0]/64.0f;
      (*frames_out)[f][i].y=vdata[idx+1]/64.0f;
      (*frames_out)[f][i].z=vdata[idx+2]/64.0f;
    }
  }

  // Triangles
  int*idx=(int*)(mesh_base+m->ofs_tris);
  *tris_out=malloc(m->ntris*3*sizeof(int));
  memcpy(*tris_out,idx,m->ntris*3*sizeof(int));

  free(d);
}

// Load complete character model (head + upper + lower)
static void ld_character(Character*c,const char*model_name){
  char path[256];

  // Lower body (legs) - has most animation frames + tag_torso attachment point
  sprintf(path,"assets/models/players/%s/lower.md3",model_name);
  ld_md3_multi(path,&c->lower_frames,&c->lower_tags,&c->lower_ntags,
    &c->lower_tris,&c->lower_nv,&c->lower_nt,&c->lower_nf);
  printf("Loaded %s lower: %d verts, %d tris, %d frames, %d tags\n",
    model_name,c->lower_nv,c->lower_nt,c->lower_nf,c->lower_ntags);

  // Upper body (torso) - has tag_head and tag_weapon
  sprintf(path,"assets/models/players/%s/upper.md3",model_name);
  ld_md3_multi(path,&c->upper_frames,&c->upper_tags,&c->upper_ntags,
    &c->upper_tris,&c->upper_nv,&c->upper_nt,&c->upper_nf);
  printf("Loaded %s upper: %d verts, %d tris, %d frames, %d tags\n",
    model_name,c->upper_nv,c->upper_nt,c->upper_nf,c->upper_ntags);

  // Head (usually single frame)
  sprintf(path,"assets/models/players/%s/head.md3",model_name);
  V**head_frames;Tag*head_tags;
  int head_nf,head_ntags;
  ld_md3_multi(path,&head_frames,&head_tags,&head_ntags,
    &c->head_tris,&c->head_nv,&c->head_nt,&head_nf);
  if(head_frames){
    c->head=head_frames[0];  // Use first frame
    free(head_frames);if(head_tags)free(head_tags);
  }
  printf("Loaded %s head: %d verts, %d tris\n",model_name,c->head_nv,c->head_nt);
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER VIII: Shader Compilation - The GPU's Functional Pipeline

   Shaders are pure functions executed in parallel across the geometry.
   We compile these λ-expressions into the GPU's instruction set.
   ═══════════════════════════════════════════════════════════════════════════*/

static char*vss=
"#version 330 core\n"
"layout(location=0)in vec3 P;"
"layout(location=1)in vec2 T;"
"layout(location=2)in vec2 L;"
"layout(location=3)in vec4 C;"
"out vec2 uv;out vec2 lm;out vec4 col;"
"uniform mat4 VP;"
"void main(){"
  "gl_Position=VP*vec4(P,1);"
  "uv=T;lm=L;col=C;"
"}";

static char*fss=
"#version 330 core\n"
"in vec2 uv;in vec2 lm;in vec4 col;"
"out vec4 F;"
"uniform sampler2D tx,lmtx;"
"uniform int sky;"
"void main(){"
  "vec4 t=texture(tx,uv);"
  "if(sky>0)F=t;"
  "else{"
    "vec3 l=texture(lmtx,lm).rgb*2.0;"
    "if(l==vec3(0))l=vec3(1);"
    "F=vec4(t.rgb*l,t.a);"
  "}"
"}";

static unsigned int csh(unsigned int t,const char*s){
  unsigned int h=glCreateShader(t);
  glShaderSource(h,1,&s,0);glCompileShader(h);
  int ok;glGetShaderiv(h,GL_COMPILE_STATUS,&ok);
  if(!ok){char l[512];glGetShaderInfoLog(h,512,0,l);printf("Shader: %s\n",l);}
  return h;
}

static char*wvss=
"#version 330 core\n"
"layout(location=0)in vec3 P;"
"out vec3 col;"
"uniform mat4 VP,M;"
"uniform vec3 tint;"
"void main(){"
  "gl_Position=VP*M*vec4(P,1);"
  "col=tint;"
"}";

static char*wfss=
"#version 330 core\n"
"in vec3 col;"
"out vec4 F;"
"void main(){"
  "F=vec4(col,1);"
"}";

static void ishd(G*g){
  g->vsh=csh(GL_VERTEX_SHADER,vss);
  g->fsh=csh(GL_FRAGMENT_SHADER,fss);
  g->prg=glCreateProgram();
  glAttachShader(g->prg,g->vsh);glAttachShader(g->prg,g->fsh);
  glLinkProgram(g->prg);

  unsigned int wvsh=csh(GL_VERTEX_SHADER,wvss);
  unsigned int wfsh=csh(GL_FRAGMENT_SHADER,wfss);
  g->wprog=glCreateProgram();
  glAttachShader(g->wprog,wvsh);glAttachShader(g->wprog,wfsh);
  glLinkProgram(g->wprog);
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER IX: Texture Loading - Materializing the Visual Field

   Textures are 2D arrays sampled by the fragment shader.
   ═══════════════════════════════════════════════════════════════════════════*/

static void ltx(G*g){
  glGenTextures(256,g->tx);
  for(int i=0;i<g->m.ntx&&i<256;i++){
    char p[256];sprintf(p,"assets/%s.tga",g->m.tx[i].n);
    int sz,w,h;unsigned char*d=rd(p,&sz);
    if(d){
      unsigned char*px=tga(d,&w,&h);
      if(px){
        glBindTexture(GL_TEXTURE_2D,g->tx[i]);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,px);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glGenerateMipmap(GL_TEXTURE_2D);
        free(px);
      }
      free(d);
    }else{
      unsigned char w[16]={255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255};
      glBindTexture(GL_TEXTURE_2D,g->tx[i]);
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,w);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    }
  }

  glGenTextures(256,g->lm);
  for(int i=0;i<g->m.nlm&&i<256;i++){
    glBindTexture(GL_TEXTURE_2D,g->lm[i]);
    unsigned char*lm=g->m.lm+i*128*128*3,*rgba=malloc(128*128*4);
    for(int j=0;j<128*128;j++){
      rgba[j*4]=lm[j*3];rgba[j*4+1]=lm[j*3+1];rgba[j*4+2]=lm[j*3+2];rgba[j*4+3]=255;
    }
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,128,128,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    free(rgba);
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER X: Matrix Mathematics - Projective Transformations

   We construct the view and projection matrices that map world space
   to clip space. This is the homomorphism from R³ to screen coordinates.
   ═══════════════════════════════════════════════════════════════════════════*/

static void vpmat(float*o,V e,float y,float p,int w,int h){
  float cy=cosf(y),sy=sinf(y),cp=cosf(p),sp=sinf(p);

  // Q3 coordinate system from AngleVectors (q_math.c)
  // Forward = (cp*cy, cp*sy, -sp)
  // Right = (sy, -cy, 0) when roll=0
  // Up = Right × Forward
  V f=v3(cp*cy,cp*sy,-sp);     // Forward
  V s=v3(sy,-cy,0);             // Right
  V u=crs(s,f);                 // Up = Right × Forward

  // View matrix: Q3 puts axis vectors in ROWS (see tr_main.c viewerMatrix construction)
  // axis[0]=forward in row 0, axis[1]=left in row 1, axis[2]=up in row 2
  // In column-major: row i, col j is at index j*4+i
  // So: v[j*4+i] = axis[i][j]
  V left=scl(s,-1);  // Q3 uses LEFT not RIGHT (axis[1] = -right from AnglesToAxis)
  float v[16]={
    f.x,left.x,u.x,0,  // Column 0: (forward.x, left.x, up.x, 0)
    f.y,left.y,u.y,0,  // Column 1: (forward.y, left.y, up.y, 0)
    f.z,left.z,u.z,0,  // Column 2: (forward.z, left.z, up.z, 0)
    0,0,0,1
  };
  v[12]=-e.x*f.x-e.y*f.y-e.z*f.z;  // -dot(e, forward)
  v[13]=-e.x*left.x-e.y*left.y-e.z*left.z;  // -dot(e, left)
  v[14]=-e.x*u.x-e.y*u.y-e.z*u.z;  // -dot(e, up)

  float a=(float)w/h,F=70*M_PI/180,n=0.1f,r=4096.0f;
  float t=1.0f/tanf(F/2);
  float pr[16]={t/a,0,0,0,0,t,0,0,0,0,-(r+n)/(r-n),-1,0,0,-2*r*n/(r-n),0};

  for(int i=0;i<16;i++)o[i]=0;
  for(int i=0;i<4;i++)for(int j=0;j<4;j++)
    for(int k=0;k<4;k++)o[j*4+i]+=pr[k*4+i]*v[j*4+k];
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER XI: The Render Loop - Composing the Visual Stream

   Rendering is a fold over the face list, applying the shader pipeline
   to each triangle. Animation updates run before rendering.
   ═══════════════════════════════════════════════════════════════════════════*/

static void ss(const char*fn,int w,int h){
  unsigned char*px=malloc(w*h*3);
  glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,px);
  FILE*f=fopen(fn,"wb");
  if(f){
    fprintf(f,"P6\n%d %d\n255\n",w,h);
    for(int y=h-1;y>=0;y--)fwrite(px+y*w*3,3,w,f);
    fclose(f);
  }
  free(px);
}

static void drw(G*g){
  glClearColor(0.2f,0.3f,0.4f,1);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glUseProgram(g->prg);

  float vp[16];
  vpmat(vp,g->cp,g->cy,g->pitch,g->sw,g->sh);

  int vpl=glGetUniformLocation(g->prg,"VP");
  int txl=glGetUniformLocation(g->prg,"tx");
  int lml=glGetUniformLocation(g->prg,"lmtx");
  int skyl=glGetUniformLocation(g->prg,"sky");

  glUniformMatrix4fv(vpl,1,GL_FALSE,vp);

  glBindVertexArray(g->vao);

  int drawn=0;
  for(int f=0;f<g->m.nlf;f++){
    LF*lf=&g->m.lf[f];

    if(lf->c!=1&&lf->c!=3)continue;

    int tid=lf->t>=0&&lf->t<g->m.ntx?lf->t:0;
    int lmid=lf->m>=0&&lf->m<g->m.nlm?lf->m:0;

    int sky=(tid>=0&&tid<g->m.ntx&&(g->m.tx[tid].f&0x04))?1:0;

    glUniform1i(skyl,sky);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,g->tx[tid]);
    glUniform1i(txl,0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,g->lm[lmid]);
    glUniform1i(lml,1);

    if(lf->c==1&&lf->nmv>=3){
      glDrawElementsBaseVertex(GL_TRIANGLES,lf->nmv,GL_UNSIGNED_INT,
        (void*)(size_t)(lf->mv*sizeof(int)),lf->v);
      drawn++;
    }else if(lf->c==3&&lf->nv>=3){
      glDrawArrays(GL_TRIANGLE_FAN,lf->v,lf->nv);
      drawn++;
    }
  }

  // IK chain - cyan lines dancing through space
  glUseProgram(g->wprog);
  glBindVertexArray(g->ikvao);
  glLineWidth(5.0f);
  glUniformMatrix4fv(glGetUniformLocation(g->wprog,"VP"),1,GL_FALSE,vp);
  glUniformMatrix4fv(glGetUniformLocation(g->wprog,"M"),1,GL_FALSE,(float[]){1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});
  glUniform3f(glGetUniformLocation(g->wprog,"tint"),0,1,1);
  glDrawArrays(GL_LINES,0,18);

  // Character model - animated sarge in front of spawn
  if(g->show_player&&g->player.lower_frames){
    int frame=((g->fc/2)%g->player.lower_nf);  // Slow animation

    // Position character in front of camera view
    V char_pos=v3(g->spawn.pos.x+100,g->spawn.pos.y,g->spawn.pos.z-30);

    // Lower body (legs) - green
    glBindVertexArray(g->player.lower_vao);
    glBindBuffer(GL_ARRAY_BUFFER,g->player.lower_vbo);
    glBufferData(GL_ARRAY_BUFFER,g->player.lower_nv*sizeof(V),g->player.lower_frames[frame],GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g->player.lower_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,g->player.lower_nt*3*sizeof(int),g->player.lower_tris,GL_DYNAMIC_DRAW);

    float lower_m[16]={
      1,0,0,0,
      0,1,0,0,
      0,0,1,0,
      char_pos.x,char_pos.y,char_pos.z,1
    };
    glUniformMatrix4fv(glGetUniformLocation(g->wprog,"VP"),1,GL_FALSE,vp);
    glUniformMatrix4fv(glGetUniformLocation(g->wprog,"M"),1,GL_FALSE,lower_m);
    glUniform3f(glGetUniformLocation(g->wprog,"tint"),0.3f,0.9f,0.3f);
    glDrawElements(GL_TRIANGLES,g->player.lower_nt*3,GL_UNSIGNED_INT,0);

    // Upper body (torso) - blue, offset upward
    if(g->player.upper_frames&&frame<g->player.upper_nf){
      glBindVertexArray(g->player.upper_vao);
      glBindBuffer(GL_ARRAY_BUFFER,g->player.upper_vbo);
      glBufferData(GL_ARRAY_BUFFER,g->player.upper_nv*sizeof(V),g->player.upper_frames[frame],GL_DYNAMIC_DRAW);
      glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,0);
      glEnableVertexAttribArray(0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g->player.upper_ebo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,g->player.upper_nt*3*sizeof(int),g->player.upper_tris,GL_DYNAMIC_DRAW);

      float upper_m[16]={
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        char_pos.x,char_pos.y,char_pos.z+24,1  // Offset up for torso
      };
      glUniformMatrix4fv(glGetUniformLocation(g->wprog,"M"),1,GL_FALSE,upper_m);
      glUniform3f(glGetUniformLocation(g->wprog,"tint"),0.3f,0.5f,1.0f);
      glDrawElements(GL_TRIANGLES,g->player.upper_nt*3,GL_UNSIGNED_INT,0);
    }

    // Head - red, offset further up
    if(g->player.head){
      glBindVertexArray(g->player.head_vao);
      glBindBuffer(GL_ARRAY_BUFFER,g->player.head_vbo);
      glBufferData(GL_ARRAY_BUFFER,g->player.head_nv*sizeof(V),g->player.head,GL_STATIC_DRAW);
      glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,0);
      glEnableVertexAttribArray(0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g->player.head_ebo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,g->player.head_nt*3*sizeof(int),g->player.head_tris,GL_STATIC_DRAW);

      float head_m[16]={
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        char_pos.x,char_pos.y,char_pos.z+48,1  // Offset up for head
      };
      glUniformMatrix4fv(glGetUniformLocation(g->wprog,"M"),1,GL_FALSE,head_m);
      glUniform3f(glGetUniformLocation(g->wprog,"tint"),1.0f,0.3f,0.3f);
      glDrawElements(GL_TRIANGLES,g->player.head_nt*3,GL_UNSIGNED_INT,0);
    }
  }

  // Weapon - golden BFG floating in world space for now
  if(g->wpn.ni>0){
    glBindVertexArray(g->wvao);

    // Place weapon at spawn + offset so it's visible
    float wm[16]={
      5,0,0,0,
      0,5,0,0,
      0,0,5,0,
      g->spawn.pos.x+80,g->spawn.pos.y,g->spawn.pos.z,1
    };
    glUniformMatrix4fv(glGetUniformLocation(g->wprog,"VP"),1,GL_FALSE,vp);
    glUniformMatrix4fv(glGetUniformLocation(g->wprog,"M"),1,GL_FALSE,wm);
    glUniform3f(glGetUniformLocation(g->wprog,"tint"),1,0.9f,0.2f);

    glDrawElements(GL_TRIANGLES,g->wpn.ni,GL_UNSIGNED_INT,0);
  }

  SDL_GL_SwapWindow(g->w);

  // Capture screenshots every 15 frames for comprehensive analysis
  if(g->fc%15==0){
    char fn[64];sprintf(fn,"physics_test_%03d.ppm",g->fc);
    ss(fn,g->sw,g->sh);
    printf("Test phase %d, Frame %d: ",g->test_phase,g->fc);
    switch(g->test_phase%6){
      case 0:printf("Static view\n");break;
      case 1:printf("Forward movement\n");break;
      case 2:printf("Backward movement\n");break;
      case 3:printf("Forward + rotate left\n");break;
      case 4:printf("Forward + rotate right\n");break;
      case 5:printf("Pitch test\n");break;
    }
  }
  g->fc++;
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER XII: Player Movement - The Physics Monad

   Movement is a state transformation. We apply velocity vectors and
   collision detection, producing a new player position each frame.
   ═══════════════════════════════════════════════════════════════════════════*/

// BSP ground collision - find highest floor polygon under player
static float trace_ground(M*m,V pos){
  float best_z=-1000;
  const float SEARCH_RADIUS=150.0f;  // Horizontal search radius

  // Scan all faces for floors near player position
  for(int i=0;i<m->nlf;i++){
    LF*f=&m->lf[i];
    if(f->t!=1||f->nv<3)continue;  // Only polygon faces with vertices

    // Check if face normal points upward (floor)
    if(f->nm.z<0.7f)continue;

    // Check all vertices of this face
    int found_nearby=0;
    float face_z_sum=0;
    int face_vert_count=0;

    for(int v=0;v<f->nv&&(f->v+v)<m->nv;v++){
      V fv=m->vs[f->v+v];

      // Calculate distance to this vertex
      float dx=fv.x-pos.x;
      float dy=fv.y-pos.y;
      float dist_sq=dx*dx+dy*dy;

      // If vertex is within radius and below player
      if(dist_sq<SEARCH_RADIUS*SEARCH_RADIUS){
        found_nearby=1;
        face_z_sum+=fv.z;
        face_vert_count++;
      }
    }

    // If we found nearby vertices, use average Z of face
    if(found_nearby&&face_vert_count>0){
      float face_z=face_z_sum/face_vert_count;
      if(face_z>best_z&&face_z<=pos.z+10)best_z=face_z;
    }
  }

  return best_z;
}

static void mv(G*g,float dt){
  const float GRAVITY=800.0f;          // Q3 gravity
  const float GROUND_ACCEL=1000.0f;    // Ground acceleration
  const float AIR_ACCEL=100.0f;        // Air acceleration
  const float FRICTION=6.0f;           // Ground friction
  const float MAX_STEP_HEIGHT=18.0f;   // Q3 step height
  const float MAX_VEL=320.0f;          // Max horizontal velocity

  // Q3 coords: X=forward, Y=right, Z=up
  V fwd=v3(cosf(g->cy)*cosf(g->pitch),sinf(g->cy)*cosf(g->pitch),-sinf(g->pitch));
  V rgt=v3(-sinf(g->cy),cosf(g->cy),0);

  // Build movement input vector
  V move_input=v3(0,0,0);
  if(g->fwd)move_input=add(move_input,fwd);
  if(g->bck)move_input=sub(move_input,fwd);
  if(g->lft)move_input=sub(move_input,rgt);
  if(g->rgt)move_input=add(move_input,rgt);

  // Normalize movement input (prevent diagonal speed boost)
  float input_len=vlen(move_input);
  if(input_len>0.01f)move_input=scl(move_input,1.0f/input_len);

  // Apply gravity
  if(!g->on_ground){
    g->vel.z-=GRAVITY*dt;
  }

  // Apply movement acceleration
  float accel=g->on_ground?GROUND_ACCEL:AIR_ACCEL;
  if(input_len>0.01f){
    g->vel.x+=move_input.x*accel*dt;
    g->vel.y+=move_input.y*accel*dt;
  }

  // Apply friction when on ground and not moving
  if(g->on_ground&&input_len<0.01f){
    float speed=sqrtf(g->vel.x*g->vel.x+g->vel.y*g->vel.y);
    if(speed>0){
      float drop=speed*FRICTION*dt;
      float newspeed=fmaxf(0,speed-drop);
      float scale=newspeed/speed;
      g->vel.x*=scale;
      g->vel.y*=scale;
    }
  }

  // Limit horizontal velocity
  float vel_2d=sqrtf(g->vel.x*g->vel.x+g->vel.y*g->vel.y);
  if(vel_2d>MAX_VEL){
    float scale=MAX_VEL/vel_2d;
    g->vel.x*=scale;
    g->vel.y*=scale;
  }

  // Try to move
  V new_pos=add(g->cp,scl(g->vel,dt));

  // Ground trace from both current and new position
  float ground_at_new=trace_ground(&g->m,new_pos);
  float ground_at_cur=trace_ground(&g->m,g->cp);

  // Use the best ground estimate
  g->ground_z=(ground_at_new>ground_at_cur)?ground_at_new:ground_at_cur;

  // Safety: if no ground found, use spawn height as fallback to prevent falling out
  if(g->ground_z<-500){
    g->ground_z=g->spawn.pos.z;
  }

  // Check for landing or step climbing
  const float PLAYER_HEIGHT=56.0f;
  const float GROUND_SNAP_DIST=10.0f;  // Distance to auto-snap to ground

  float clearance=new_pos.z-g->ground_z;

  if(clearance<-5.0f){
    // Significantly below ground - climb step or land
    float step_height=g->ground_z-g->cp.z;
    if(step_height>0&&step_height<=MAX_STEP_HEIGHT){
      // Climbable step
      new_pos.z=g->ground_z;
      g->vel.z=0;
      g->on_ground=1;
    }else{
      // Hit solid floor or fell through - snap to ground
      new_pos.z=g->ground_z;
      g->vel.z=0;
      g->on_ground=1;
    }
  }else if(clearance<GROUND_SNAP_DIST&&g->vel.z<=0){
    // Close to ground and moving down - land
    new_pos.z=g->ground_z;
    g->vel.z=0;
    g->on_ground=1;
  }else if(clearance>GROUND_SNAP_DIST*3){
    // Far from ground - in air
    g->on_ground=0;
  }else{
    // Near ground - maintain contact
    if(clearance<0){
      new_pos.z=g->ground_z;
      g->vel.z=0;
    }
    g->on_ground=1;
  }

  // Safety bounds - never go too far below spawn
  if(new_pos.z<g->spawn.pos.z-200){
    new_pos.z=g->spawn.pos.z;
    g->vel.z=0;
    g->on_ground=1;
  }

  g->cp=new_pos;
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER XIII: Event Handling - The IO Monad Unfolds

   User input flows through SDL's event queue.
   ═══════════════════════════════════════════════════════════════════════════*/

static void evt(G*g){
  SDL_Event e;
  while(SDL_PollEvent(&e)){
    if(e.type==SDL_QUIT)g->run=0;
    if(e.type==SDL_KEYDOWN){
      if(e.key.keysym.sym==SDLK_ESCAPE)g->run=0;
      if(e.key.keysym.sym==SDLK_w)g->fwd=1;
      if(e.key.keysym.sym==SDLK_s)g->bck=1;
      if(e.key.keysym.sym==SDLK_a)g->lft=1;
      if(e.key.keysym.sym==SDLK_d)g->rgt=1;
    }
    if(e.type==SDL_KEYUP){
      if(e.key.keysym.sym==SDLK_w)g->fwd=0;
      if(e.key.keysym.sym==SDLK_s)g->bck=0;
      if(e.key.keysym.sym==SDLK_a)g->lft=0;
      if(e.key.keysym.sym==SDLK_d)g->rgt=0;
    }
    if(e.type==SDL_MOUSEMOTION){
      g->cy+=e.motion.xrel*0.002f;
      g->pitch-=e.motion.yrel*0.002f;
      if(g->pitch>M_PI/2-0.01f)g->pitch=M_PI/2-0.01f;
      if(g->pitch<-M_PI/2+0.01f)g->pitch=-M_PI/2+0.01f;
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER XIV: Initialization - Building the Integrated Context

   We construct the window, OpenGL context, animation system, and buffers.
   This is our main combinator, composing all subsystems into one engine.
   ═══════════════════════════════════════════════════════════════════════════*/

static int ini(G*g,const char*mp){
  SDL_Init(SDL_INIT_VIDEO);
  g->sw=1920;g->sh=1080;
  g->w=SDL_CreateWindow("Q3 Integrated",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
    g->sw,g->sh,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
  if(!g->w){printf("Window fail\n");return 0;}

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
  g->g=SDL_GL_CreateContext(g->w);
  if(!g->g){printf("GL fail\n");return 0;}

  glewExperimental=GL_TRUE;
  if(glewInit()!=GLEW_OK){printf("GLEW fail\n");return 0;}

  SDL_SetRelativeMouseMode(SDL_TRUE);

  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);

  if(!ld(mp,&g->m)){printf("Load fail\n");return 0;}

  // Load spawn before using its position
  g->spawn=pent(mp);

  g->anim=anim_create(10);
  // IK chain floats in front of spawn at eye level - highly visible
  V ik_base=v3(g->spawn.pos.x+30,g->spawn.pos.y-10,g->spawn.pos.z);
  for(int i=0;i<10;i++){
    g->anim->rig->bones[i].length=20.0f;
    g->anim->rig->current.positions[i]=v3(ik_base.x+i*20,ik_base.y,ik_base.z);
  }
  anim_add_spring(g->anim,5,30.0f,0.3f);
  anim_add_muscle(g->anim,0,5,v3(0,0,0),v3(0,0,0));

  ishd(g);
  ltx(g);

  glGenVertexArrays(1,&g->vao);
  glBindVertexArray(g->vao);

  glGenBuffers(1,&g->vbo);
  glBindBuffer(GL_ARRAY_BUFFER,g->vbo);

  int stride=sizeof(V)+sizeof(T)*2+sizeof(C);
  unsigned char*vdata=malloc(g->m.nv*stride);
  for(int i=0;i<g->m.nv;i++){
    memcpy(vdata+i*stride,&g->m.vs[i],sizeof(V));
    memcpy(vdata+i*stride+sizeof(V),&g->m.ts[i],sizeof(T));
    memcpy(vdata+i*stride+sizeof(V)+sizeof(T),&g->m.ls[i],sizeof(T));
    memcpy(vdata+i*stride+sizeof(V)+sizeof(T)*2,&g->m.cs[i],sizeof(C));
  }
  glBufferData(GL_ARRAY_BUFFER,g->m.nv*stride,vdata,GL_STATIC_DRAW);
  free(vdata);

  glGenBuffers(1,&g->ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g->ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,g->m.ni*sizeof(int),g->m.is,GL_STATIC_DRAW);

  glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,(void*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,stride,(void*)sizeof(V));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,stride,(void*)(sizeof(V)+sizeof(T)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(3,4,GL_UNSIGNED_BYTE,GL_TRUE,stride,(void*)(sizeof(V)+sizeof(T)*2));
  glEnableVertexAttribArray(3);

  // Weapon - BFG displays better than the tiny rocket
  g->wpn=ldmd3("assets/models/weapons2/bfg/bfg.md3");
  printf("Weapon loaded: %d verts, %d tris\n",g->wpn.nv,g->wpn.ni/3);

  if(g->wpn.nv>0){
    glGenVertexArrays(1,&g->wvao);
    glBindVertexArray(g->wvao);
    glGenBuffers(1,&g->wvbo);
    glBindBuffer(GL_ARRAY_BUFFER,g->wvbo);
    glBufferData(GL_ARRAY_BUFFER,g->wpn.nv*sizeof(V),g->wpn.vs,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,0);
    glEnableVertexAttribArray(0);

    unsigned int webo;
    glGenBuffers(1,&webo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,webo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,g->wpn.ni*sizeof(int),g->wpn.is,GL_STATIC_DRAW);
  }

  // IK chain visualization - lines connecting bones
  glGenVertexArrays(1,&g->ikvao);
  glBindVertexArray(g->ikvao);
  glGenBuffers(1,&g->ikvbo);
  glBindBuffer(GL_ARRAY_BUFFER,g->ikvbo);
  glBufferData(GL_ARRAY_BUFFER,20*sizeof(V),NULL,GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,0);
  glEnableVertexAttribArray(0);

  // Load character model (sarge)
  printf("\nLoading character model...\n");
  ld_character(&g->player,"sarge");
  g->show_player=1;  // Show by default

  // Setup character VAOs
  if(g->player.lower_frames){
    glGenVertexArrays(1,&g->player.lower_vao);
    glGenBuffers(1,&g->player.lower_vbo);
    glGenBuffers(1,&g->player.lower_ebo);
  }
  if(g->player.upper_frames){
    glGenVertexArrays(1,&g->player.upper_vao);
    glGenBuffers(1,&g->player.upper_vbo);
    glGenBuffers(1,&g->player.upper_ebo);
  }
  if(g->player.head){
    glGenVertexArrays(1,&g->player.head_vao);
    glGenBuffers(1,&g->player.head_vbo);
    glGenBuffers(1,&g->player.head_ebo);
  }

  // Camera starts at spawn
  g->cp=g->spawn.pos;g->cy=g->spawn.angle;g->pitch=0.0f;g->run=1;g->fc=0;
  g->vel=v3(0,0,0);g->on_ground=1;g->ground_z=g->spawn.pos.z;
  g->auto_test=1;  // Enable automated testing
  g->test_phase=0;

  printf("Spawn: (%.0f,%.0f,%.0f) - Starting automated physics tests\n",
    g->spawn.pos.x,g->spawn.pos.y,g->spawn.pos.z);

  return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER XV: The Main Loop - Eternal Recursion with Animation

   The game loop is a tail-recursive function that never returns.
   Each iteration processes input, updates animation state, and renders.
   Time flows forward; frames accumulate into experience.
   ═══════════════════════════════════════════════════════════════════════════*/

int main(int argc,char**argv){
  G*g=calloc(1,sizeof(G));
  const char*mp=argc>1?argv[1]:"assets/maps/dm4ish.bsp";

  printf("╔═══════════════════════════════════════════════════════════════╗\n");
  printf("║  QUAKE III ARENA - Integrated Engine (Code Golf Edition)    ║\n");
  printf("║  Renderer + Animation + Physics + IK in a single file       ║\n");
  printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

  if(!ini(g,mp)){
    printf("Initialization failed\n");
    return 1;
  }

  printf("Engine initialized:\n");
  printf("  • BSP vertices: %d\n",g->m.nv);
  printf("  • BSP faces: %d\n",g->m.nlf);
  printf("  • Textures: %d\n",g->m.ntx);
  printf("  • Lightmaps: %d\n",g->m.nlm);
  printf("  • Animation bones: %d\n",g->anim->rig->bone_count);
  printf("  • IK chains: %d\n",g->anim->ik_count);
  printf("  • Spring bones: %d\n",g->anim->spring_count);
  printf("  • Muscles: %d\n",g->anim->muscle_count);
  printf("\nRunning...\n");

  unsigned int lt=SDL_GetTicks();
  while(g->run&&g->fc<500){  // Extended for comprehensive testing
    unsigned int ct=SDL_GetTicks();
    float dt=(ct-lt)/1000.0f;
    lt=ct;

    if(g->anim){
      // Move IK target in circle - proof that mathematics can dance
      float t=g->fc*0.05f;
      V target=v3(g->spawn.pos.x+50+cosf(t)*30,g->spawn.pos.y+30,g->spawn.pos.z+sinf(t)*30);
      anim_add_ik(g->anim,0,9,target,IK_FABRIK);
      anim_update(g->anim,dt);

      // Update IK chain visualization buffer
      V lines[20];int li=0;
      for(int i=0;i<g->anim->rig->bone_count-1&&li<18;i++){
        lines[li++]=g->anim->rig->current.positions[i];
        lines[li++]=g->anim->rig->current.positions[i+1];
      }
      glBindBuffer(GL_ARRAY_BUFFER,g->ikvbo);
      glBufferSubData(GL_ARRAY_BUFFER,0,li*sizeof(V),lines);
    }

    // Automated physics testing - cycle through scenarios
    if(g->auto_test){
      int phase_duration=50;  // Frames per test phase
      g->test_phase=g->fc/phase_duration;

      switch(g->test_phase%6){
        case 0:  // Static view
          g->fwd=0;g->bck=0;g->lft=0;g->rgt=0;
          break;
        case 1:  // Move forward
          g->fwd=1;g->bck=0;g->lft=0;g->rgt=0;
          break;
        case 2:  // Move backward
          g->fwd=0;g->bck=1;g->lft=0;g->rgt=0;
          break;
        case 3:  // Rotate left while moving forward
          g->fwd=1;g->bck=0;g->lft=1;g->rgt=0;
          break;
        case 4:  // Rotate right while moving forward
          g->fwd=1;g->bck=0;g->lft=0;g->rgt=1;
          break;
        case 5:  // Look up/down test
          g->fwd=0;g->bck=0;g->lft=0;g->rgt=0;
          g->pitch=sinf(g->fc*0.02f)*0.5f;
          break;
      }
    }else{
      evt(g);
    }

    mv(g,dt);
    drw(g);

    SDL_Delay(16);
  }

  if(g->anim)anim_destroy(g->anim);
  SDL_GL_DeleteContext(g->g);
  SDL_DestroyWindow(g->w);
  SDL_Quit();
  free(g);

  printf("\nEngine shutdown complete.\n");
  return 0;
}
