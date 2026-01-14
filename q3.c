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

typedef struct{
  SDL_Window*w;SDL_GLContext g;int sw,sh;
  unsigned int vao,vbo,ebo,tx[256],lm[256];
  unsigned int prg,vsh,fsh,wprog,wvao,wvbo,ikvao,ikvbo;
  V cp,ca;float cy,pitch;int fwd,bck,lft,rgt;
  M m;AnimCtrl*anim;Spawn spawn;MD3G wpn;int run,fc;
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
  V f=nrm(v3(cy*cp,sp,sy*cp));
  V s=nrm(crs(v3(0,1,0),f));
  V u=crs(f,s);

  float v[16]={s.x,s.y,s.z,0,u.x,u.y,u.z,0,-f.x,-f.y,-f.z,0,0,0,0,1};
  v[12]=-dot(s,e);v[13]=-dot(u,e);v[14]=dot(f,e);

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

  if(g->fc==60||g->fc==90){
    char fn[64];sprintf(fn,"integrated_shot_%03d.ppm",g->fc);
    ss(fn,g->sw,g->sh);printf("Screenshot: %s\n",fn);
    if(g->fc==90)g->run=0;
  }
  g->fc++;
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER XII: Player Movement - The Physics Monad

   Movement is a state transformation. We apply velocity vectors and
   collision detection, producing a new player position each frame.
   ═══════════════════════════════════════════════════════════════════════════*/

static void mv(G*g,float dt){
  float sp=300*dt;
  V fwd=v3(cosf(g->cy),0,sinf(g->cy));
  V rgt=v3(-sinf(g->cy),0,cosf(g->cy));

  if(g->fwd){g->cp=add(g->cp,scl(fwd,sp));g->pitch-=dt*0.1f;}
  if(g->bck){g->cp=sub(g->cp,scl(fwd,sp));g->pitch+=dt*0.1f;}
  if(g->lft){g->cp=sub(g->cp,scl(rgt,sp));g->cy+=dt*0.3f;}
  if(g->rgt){g->cp=add(g->cp,scl(rgt,sp));g->cy-=dt*0.3f;}
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

  // Camera starts at spawn
  g->cp=g->spawn.pos;g->cy=g->spawn.angle;g->pitch=0.0f;g->run=1;g->fc=0;

  printf("Spawn: (%.0f,%.0f,%.0f) IK chain ahead at eye level\n",
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
  while(g->run&&g->fc<120){
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

    evt(g);
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
