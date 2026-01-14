/* Proper MD3 Articulated Character Rendering

   Based on Q3 source code research:
   - MD3 files contain tags (position + 3x3 rotation matrix)
   - Character parts attach via tags: legs->torso->head
   - CG_PositionRotatedEntityOnTag handles attachment
*/

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Q3 angle indices
#define PITCH 0
#define YAW   1
#define ROLL  2

typedef struct{float x,y,z;}V;
typedef struct{float u,v;}T;

// MD3 tag - attachment point with rotation
typedef struct{
  char name[64];
  V origin;
  float axis[3][3];  // 3x3 rotation matrix
}Tag;

// MD3 mesh with tags for articulation
typedef struct{
  V**frames;      // frames[frame][vert]
  T*uvs;
  int*tris;
  Tag*tags;       // Tags for this model
  int nverts,ntris,nframes,ntags;
  char shader[64];
}MD3;

// Complete articulated character
typedef struct{
  MD3 lower,upper,head;
  int lower_frame,upper_frame;
}Character;

// Vector ops
static inline V v3(float x,float y,float z){return(V){x,y,z};}
static inline V add(V a,V b){return v3(a.x+b.x,a.y+b.y,a.z+b.z);}
static inline V sub(V a,V b){return v3(a.x-b.x,a.y-b.y,a.z-b.z);}
static inline V scl(V a,float s){return v3(a.x*s,a.y*s,a.z*s);}
static inline float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static inline float vlen(V a){return sqrtf(dot(a,a));}
static inline V nrm(V a){float l=vlen(a);return l>1e-6f?scl(a,1.0f/l):a;}
static inline V crs(V a,V b){return v3(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}

// Matrix multiply vector
static V mat_mul_vec(float m[3][3],V v){
  return v3(
    m[0][0]*v.x+m[0][1]*v.y+m[0][2]*v.z,
    m[1][0]*v.x+m[1][1]*v.y+m[1][2]*v.z,
    m[2][0]*v.x+m[2][1]*v.y+m[2][2]*v.z
  );
}

// Matrix multiply matrix
static void mat_mul_mat(float a[3][3],float b[3][3],float out[3][3]){
  for(int i=0;i<3;i++){
    for(int j=0;j<3;j++){
      out[i][j]=0;
      for(int k=0;k<3;k++){
        out[i][j]+=a[i][k]*b[k][j];
      }
    }
  }
}

// Q3 AngleVectors - from q_math.c
static void angle_vectors(float angles[3],V*fwd,V*right,V*up){
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

// File I/O
static void*rd(const char*p,int*sz){
  FILE*f=fopen(p,"rb");if(!f)return 0;
  fseek(f,0,SEEK_END);*sz=ftell(f);fseek(f,0,SEEK_SET);
  void*d=malloc(*sz);fread(d,1,*sz,f);fclose(f);return d;
}

// Load MD3 with tags
static MD3 load_md3_with_tags(const char*path){
  MD3 md3={0};
  int sz;
  unsigned char*data=rd(path,&sz);
  if(!data){printf("Failed to load %s\n",path);return md3;}

  // MD3 header
  typedef struct{
    char id[4];int ver;char name[64];int flags;
    int nframes,ntags,nmeshes,nskins;
    int ofs_frames,ofs_tags,ofs_meshes,ofs_eof;
  }MD3H;

  MD3H*h=(MD3H*)data;
  if(memcmp(h->id,"IDP3",4)!=0||h->ver!=15){
    printf("Invalid MD3\n");free(data);return md3;
  }

  printf("Loading %s: %d frames, %d tags, %d meshes\n",path,h->nframes,h->ntags,h->nmeshes);

  // Load tags
  md3.ntags=h->ntags;
  md3.nframes=h->nframes;
  if(md3.ntags>0){
    md3.tags=malloc(md3.ntags*md3.nframes*sizeof(Tag));
    unsigned char*tag_data=data+h->ofs_tags;

    for(int f=0;f<md3.nframes;f++){
      for(int t=0;t<md3.ntags;t++){
        int idx=f*md3.ntags+t;
        // MD3 tag format: char name[64], float origin[3], float axis[3][3]
        char*name=(char*)(tag_data+(idx*(64+12+36)));
        float*origin=(float*)(tag_data+(idx*(64+12+36))+64);
        float*axis=(float*)(tag_data+(idx*(64+12+36))+64+12);

        strncpy(md3.tags[idx].name,name,64);
        md3.tags[idx].origin=v3(origin[0],origin[1],origin[2]);
        for(int i=0;i<3;i++){
          for(int j=0;j<3;j++){
            md3.tags[idx].axis[i][j]=axis[i*3+j];
          }
        }

        if(f==0)printf("  Tag %d: %s at (%.1f,%.1f,%.1f)\n",
          t,md3.tags[idx].name,
          md3.tags[idx].origin.x,
          md3.tags[idx].origin.y,
          md3.tags[idx].origin.z);
      }
    }
  }

  // Load first mesh
  if(h->nmeshes>0){
    typedef struct{
      char id[4];char name[64];int flags;
      int nframes,nshaders,nverts,ntris;
      int ofs_tris,ofs_shaders,ofs_st,ofs_verts,ofs_end;
    }MD3M;

    unsigned char*mesh_base=data+h->ofs_meshes;
    MD3M*mesh=(MD3M*)mesh_base;

    md3.nverts=mesh->nverts;
    md3.ntris=mesh->ntris;

    // Allocate frame arrays
    md3.frames=malloc(md3.nframes*sizeof(V*));
    for(int f=0;f<md3.nframes;f++){
      md3.frames[f]=malloc(md3.nverts*sizeof(V));
    }

    // Load vertices
    short*vdata=(short*)(mesh_base+mesh->ofs_verts);
    for(int f=0;f<md3.nframes;f++){
      for(int i=0;i<md3.nverts;i++){
        int idx=(f*md3.nverts+i)*4;
        md3.frames[f][i].x=vdata[idx+0]/64.0f;
        md3.frames[f][i].y=vdata[idx+1]/64.0f;
        md3.frames[f][i].z=vdata[idx+2]/64.0f;
      }
    }

    // Load triangles
    md3.tris=malloc(md3.ntris*3*sizeof(int));
    int*tdata=(int*)(mesh_base+mesh->ofs_tris);
    memcpy(md3.tris,tdata,md3.ntris*3*sizeof(int));

    // Load UVs
    md3.uvs=malloc(md3.nverts*sizeof(T));
    float*uvdata=(float*)(mesh_base+mesh->ofs_st);
    for(int i=0;i<md3.nverts;i++){
      md3.uvs[i].u=uvdata[i*2+0];
      md3.uvs[i].v=uvdata[i*2+1];
    }
  }

  free(data);
  return md3;
}

// Find tag by name in current frame
static Tag*find_tag(MD3*md3,int frame,const char*name){
  for(int i=0;i<md3->ntags;i++){
    int idx=frame*md3->ntags+i;
    if(strcmp(md3->tags[idx].name,name)==0){
      return &md3->tags[idx];
    }
  }
  return NULL;
}

// Transform vertex by tag (Q3 CG_PositionRotatedEntityOnTag logic)
static V transform_by_tag(V vert,Tag*tag,V parent_origin,float parent_axis[3][3]){
  // Apply tag's rotation to vertex
  V v_rotated=mat_mul_vec(tag->axis,vert);

  // Add tag's offset
  V v_offset=add(v_rotated,tag->origin);

  // Transform by parent's axis
  V v_parent=mat_mul_vec(parent_axis,v_offset);

  // Add parent's origin
  return add(v_parent,parent_origin);
}

// Screenshot
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

int main(){
  printf("Loading articulated character model...\n");

  Character ch={0};
  ch.lower=load_md3_with_tags("assets/models/players/sarge/lower.md3");
  ch.upper=load_md3_with_tags("assets/models/players/sarge/upper.md3");
  ch.head=load_md3_with_tags("assets/models/players/sarge/head.md3");

  if(ch.lower.nverts==0||ch.upper.nverts==0||ch.head.nverts==0){
    printf("Failed to load character\n");
    return 1;
  }

  // Init SDL + OpenGL
  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window*w=SDL_CreateWindow("Articulated Character",0,0,1920,1080,
    SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  SDL_GLContext ctx=SDL_GL_CreateContext(w);
  glewInit();

  glEnable(GL_DEPTH_TEST);
  glViewport(0,0,1920,1080);

  // Test articulation - find tags
  Tag*tag_torso=find_tag(&ch.lower,0,"tag_torso");
  Tag*tag_head=find_tag(&ch.upper,0,"tag_head");

  if(tag_torso)printf("\n✓ Found tag_torso at (%.1f,%.1f,%.1f)\n",
    tag_torso->origin.x,tag_torso->origin.y,tag_torso->origin.z);
  if(tag_head)printf("✓ Found tag_head at (%.1f,%.1f,%.1f)\n",
    tag_head->origin.x,tag_head->origin.y,tag_head->origin.z);

  // Render test
  glClearColor(0.2f,0.2f,0.3f,1);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  printf("\n✓ Articulated character system initialized\n");
  printf("  Tags loaded and ready for proper attachment\n\n");

  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(w);
  SDL_Quit();

  return 0;
}
