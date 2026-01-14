// MD3 Model & Animation Test Program
// Tests character models, animations, camera angles, and corner cases
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <GL/glew.h>

#define PI 3.14159265359f

// Vector math
typedef struct{float x,y,z;}V;
typedef struct{float u,v;}T;
typedef struct{unsigned char r,g,b,a;}C;

static V v3(float x,float y,float z){return (V){x,y,z};}
static V add(V a,V b){return v3(a.x+b.x,a.y+b.y,a.z+b.z);}
static V sub(V a,V b){return v3(a.x-b.x,a.y-b.y,a.z-b.z);}
static V scl(V a,float s){return v3(a.x*s,a.y*s,a.z*s);}
static float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static V crs(V a,V b){return v3(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
static V nrm(V a){float l=sqrtf(dot(a,a));return l>0?scl(a,1.0f/l):a;}

// MD3 file format structures
typedef struct{char id[4];int ver;char name[64];int flags;int nframes,ntags,nmeshes,nskins;int ofs_frames,ofs_tags,ofs_meshes,ofs_eof;}MD3H;
typedef struct{char id[4];char name[64];int flags;int nframes,nshaders,nverts,ntris;int ofs_tris,ofs_shaders,ofs_st,ofs_verts,ofs_end;}MD3M;
typedef struct{V mins,maxs;V origin;float radius;char name[16];}MD3F;
typedef struct{char name[64];int index;}MD3S;

// Multi-frame MD3 model (all animation frames)
typedef struct{
  V**frames;      // frames[frame_idx][vert_idx]
  T*uvs;          // UV coords (same for all frames)
  int*tris;       // Triangle indices
  int nverts,ntris,nframes;
  char shader[64]; // Texture name from first shader
}MD3Model;

// Animation definition from animation.cfg
typedef struct{
  int first,count,loop;
  float fps;
  char name[64];
}Anim;

// Complete player model (3 parts)
typedef struct{
  MD3Model head,upper,lower;
  Anim anims[32];
  int nanim;
}Player;

// Camera state
typedef struct{
  V pos;
  float yaw,pitch;  // Angles in radians
  int w,h;
}Cam;

// Global state
typedef struct{
  SDL_Window*win;
  SDL_GLContext ctx;
  unsigned int prog,vao,vbo,ebo;
  Player player;
  Cam cam;
  int frame;
  float anim_time;
  int cur_anim;
  int screenshot_count;
}G;

// File reading
static void*rd(const char*p,int*sz){
  FILE*f=fopen(p,"rb");if(!f){printf("Can't open: %s\n",p);return 0;}
  fseek(f,0,SEEK_END);*sz=ftell(f);fseek(f,0,SEEK_SET);
  void*d=malloc(*sz);fread(d,1,*sz,f);fclose(f);return d;
}

// Load MD3 with ALL frames
static MD3Model ld_md3(const char*path){
  MD3Model m={0};
  int sz;unsigned char*d=rd(path,&sz);
  if(!d)return m;

  MD3H*h=(MD3H*)d;
  if(memcmp(h->id,"IDP3",4)||h->ver!=15){
    printf("Bad MD3: %s\n",path);free(d);return m;
  }

  printf("MD3 %s: %d frames, %d meshes\n",path,h->nframes,h->nmeshes);

  // Load first mesh only for now
  MD3M*mesh=(MD3M*)(d+h->ofs_meshes);
  m.nverts=mesh->nverts;
  m.ntris=mesh->ntris;
  m.nframes=mesh->nframes;

  printf("  Mesh: %d verts, %d tris, %d frames\n",m.nverts,m.ntris,m.nframes);

  // Allocate frame storage
  m.frames=malloc(m.nframes*sizeof(V*));

  // Vertex data starts at mesh + ofs_verts offset
  unsigned char*mesh_base=(unsigned char*)mesh;
  short*verts=(short*)(mesh_base+mesh->ofs_verts);

  for(int f=0;f<m.nframes;f++){
    m.frames[f]=malloc(m.nverts*sizeof(V));

    // Each vertex is 4 shorts: xyz[3] + normal
    for(int i=0;i<m.nverts;i++){
      int idx=(f*m.nverts+i)*4;
      m.frames[f][i].x=verts[idx+0]/64.0f;
      m.frames[f][i].y=verts[idx+1]/64.0f;
      m.frames[f][i].z=verts[idx+2]/64.0f;
    }
  }

  // UVs (same for all frames) - mesh-relative offset
  float*st=(float*)(mesh_base+mesh->ofs_st);
  m.uvs=malloc(m.nverts*sizeof(T));
  for(int i=0;i<m.nverts;i++){
    m.uvs[i].u=st[i*2];
    m.uvs[i].v=st[i*2+1];
  }

  // Triangle indices - mesh-relative offset
  int*idx=(int*)(mesh_base+mesh->ofs_tris);
  m.tris=malloc(m.ntris*3*sizeof(int));
  memcpy(m.tris,idx,m.ntris*3*sizeof(int));

  // Get shader name - mesh-relative offset
  if(mesh->nshaders>0){
    MD3S*shader=(MD3S*)(mesh_base+mesh->ofs_shaders);
    strncpy(m.shader,shader->name,63);
    m.shader[63]=0;
  }

  free(d);
  return m;
}

// Parse animation.cfg
static int parse_anims(const char*path,Anim*anims){
  FILE*f=fopen(path,"r");
  if(!f){printf("Can't open: %s\n",path);return 0;}

  char line[256];
  int n=0;

  while(fgets(line,sizeof(line),f)&&n<32){
    // Skip comments and empty lines
    if(line[0]=='/'||line[0]=='\n'||line[0]=='s')continue;

    int first,count,loop;
    float fps;
    char name[64]={0};

    // Parse: first_frame num_frames looping_frames fps // NAME
    if(sscanf(line,"%d %d %d %f",&first,&count,&loop,&fps)==4){
      // Extract animation name from comment
      char*comment=strstr(line,"//");
      if(comment){
        sscanf(comment+2," %s",name);
      }

      anims[n].first=first;
      anims[n].count=count;
      anims[n].loop=loop;
      anims[n].fps=fps;
      strncpy(anims[n].name,name,63);
      printf("Anim %d: %s frames %d-%d (%d) @ %.1ffps\n",n,name,first,first+count-1,count,fps);
      n++;
    }
  }

  fclose(f);
  return n;
}

// Load complete player model
static Player ld_player(const char*model_name){
  Player p={0};
  char path[256];

  printf("\nLoading player model: %s\n",model_name);

  sprintf(path,"assets/models/players/%s/lower.md3",model_name);
  p.lower=ld_md3(path);

  sprintf(path,"assets/models/players/%s/upper.md3",model_name);
  p.upper=ld_md3(path);

  sprintf(path,"assets/models/players/%s/head.md3",model_name);
  p.head=ld_md3(path);

  sprintf(path,"assets/models/players/%s/animation.cfg",model_name);
  p.nanim=parse_anims(path,p.anims);

  return p;
}

// Shaders
static char*vss=
"#version 330 core\n"
"layout(location=0)in vec3 P;"
"uniform mat4 VP,M;"
"uniform vec3 color;"
"out vec3 col;"
"void main(){"
"  gl_Position=VP*M*vec4(P,1);"
"  col=color;"
"}";

static char*fss=
"#version 330 core\n"
"in vec3 col;"
"out vec4 F;"
"void main(){"
"  F=vec4(col,1);"
"}";

static unsigned int csh(int type,const char*src){
  unsigned int h=glCreateShader(type);
  glShaderSource(h,1,&src,0);
  glCompileShader(h);
  int ok;glGetShaderiv(h,GL_COMPILE_STATUS,&ok);
  if(!ok){char log[512];glGetShaderInfoLog(h,512,0,log);printf("Shader: %s\n",log);}
  return h;
}

static void init_shaders(G*g){
  unsigned int vs=csh(GL_VERTEX_SHADER,vss);
  unsigned int fs=csh(GL_FRAGMENT_SHADER,fss);
  g->prog=glCreateProgram();
  glAttachShader(g->prog,vs);
  glAttachShader(g->prog,fs);
  glLinkProgram(g->prog);
  glDeleteShader(vs);
  glDeleteShader(fs);
}

// Q3 camera system: yaw/pitch to forward/right/up vectors
// Based on Q3 source: q_math.c AngleVectors()
static void angle_vectors(float yaw,float pitch,V*fwd,V*right,V*up){
  float sy=sinf(yaw),cy=cosf(yaw);
  float sp=sinf(pitch),cp=cosf(pitch);

  // Forward vector
  fwd->x=cp*cy;
  fwd->y=cp*sy;
  fwd->z=-sp;

  // Right vector (perpendicular to forward)
  right->x=sy;
  right->y=-cy;
  right->z=0;

  // Up vector (cross product)
  *up=crs(*fwd,*right);
}

// Build view-projection matrix
static void vp_matrix(float*out,Cam*cam){
  // View matrix from camera
  V fwd,right,up;
  angle_vectors(cam->yaw,cam->pitch,&fwd,&right,&up);

  // View matrix (world to camera space)
  float view[16]={
    right.x,up.x,-fwd.x,0,
    right.y,up.y,-fwd.y,0,
    right.z,up.z,-fwd.z,0,
    -dot(right,cam->pos),-dot(up,cam->pos),dot(fwd,cam->pos),1
  };

  // Projection matrix (perspective)
  float aspect=(float)cam->w/cam->h;
  float fov=70.0f*PI/180.0f;
  float f=1.0f/tanf(fov/2.0f);
  float near=1.0f,far=4096.0f;
  float proj[16]={
    f/aspect,0,0,0,
    0,f,0,0,
    0,0,(far+near)/(near-far),-1,
    0,0,(2*far*near)/(near-far),0
  };

  // Multiply: VP = P * V
  memset(out,0,16*sizeof(float));
  for(int i=0;i<4;i++)
    for(int j=0;j<4;j++)
      for(int k=0;k<4;k++)
        out[i*4+j]+=proj[i*4+k]*view[k*4+j];
}

// Screenshot as PPM
static void screenshot(const char*name,int w,int h){
  unsigned char*pix=malloc(w*h*3);
  glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pix);

  FILE*f=fopen(name,"wb");
  fprintf(f,"P6\n%d %d\n255\n",w,h);
  for(int y=h-1;y>=0;y--)fwrite(pix+y*w*3,1,w*3,f);
  fclose(f);
  free(pix);
  printf("Screenshot: %s\n",name);
}

// Render MD3 model at specific frame (interpolated)
static void render_md3(G*g,MD3Model*m,int frame1,int frame2,float lerp,V pos,V color){
  if(!m->nframes||!m->frames)return;

  // Clamp frames
  frame1=frame1%m->nframes;
  frame2=frame2%m->nframes;

  // Interpolate vertices
  V*verts=malloc(m->nverts*sizeof(V));
  for(int i=0;i<m->nverts;i++){
    V v1=m->frames[frame1][i];
    V v2=m->frames[frame2][i];
    verts[i].x=v1.x+(v2.x-v1.x)*lerp+pos.x;
    verts[i].y=v1.y+(v2.y-v1.y)*lerp+pos.y;
    verts[i].z=v1.z+(v2.z-v1.z)*lerp+pos.z;
  }

  // Debug: print first vertex
  static int dbg_count=0;
  if(dbg_count<1){
    printf("First vert: (%.1f,%.1f,%.1f) Frame %d/%d\n",verts[0].x,verts[0].y,verts[0].z,frame1,frame2);
    dbg_count++;
  }

  // Upload to GPU
  glBindVertexArray(g->vao);
  glBindBuffer(GL_ARRAY_BUFFER,g->vbo);
  glBufferData(GL_ARRAY_BUFFER,m->nverts*sizeof(V),verts,GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,g->ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,m->ntris*3*sizeof(int),m->tris,GL_DYNAMIC_DRAW);

  // Model matrix (identity - position already in vertices)
  float model[16]={
    1,0,0,0,
    0,1,0,0,
    0,0,1,0,
    0,0,0,1
  };

  // Render
  float vp[16];
  vp_matrix(vp,&g->cam);

  glUseProgram(g->prog);
  glUniformMatrix4fv(glGetUniformLocation(g->prog,"VP"),1,GL_FALSE,vp);
  glUniformMatrix4fv(glGetUniformLocation(g->prog,"M"),1,GL_FALSE,model);
  glUniform3f(glGetUniformLocation(g->prog,"color"),color.x,color.y,color.z);

  glDrawElements(GL_TRIANGLES,m->ntris*3,GL_UNSIGNED_INT,0);

  GLenum err=glGetError();
  if(err!=GL_NO_ERROR){
    printf("GL error after draw: 0x%x\n",err);
  }

  free(verts);
}

// Test scenarios with automated camera positions
static void test_scenario(G*g,const char*anim_name,const char*desc){
  // Find animation
  int anim_idx=-1;
  for(int i=0;i<g->player.nanim;i++){
    if(strstr(g->player.anims[i].name,anim_name)){
      anim_idx=i;
      break;
    }
  }

  if(anim_idx<0){
    printf("Animation '%s' not found\n",anim_name);
    return;
  }

  Anim*a=&g->player.anims[anim_idx];
  printf("\n=== Test: %s ===\n",desc);
  printf("Animation: %s (frames %d-%d)\n",a->name,a->first,a->first+a->count-1);

  // Test from multiple camera angles
  struct{float dist,yaw,pitch;const char*view;}cameras[]={
    {150,PI,0,"front"},
    {150,0,0,"back"},
    {150,PI/2,0,"right"},
    {150,-PI/2,0,"left"},
    {200,PI/4,0.3f,"angle_high"},
    {100,PI,0,"close_front"},
  };

  V model_pos=v3(0,0,0);

  for(int c=0;c<6;c++){
    // Position camera
    g->cam.yaw=cameras[c].yaw;
    g->cam.pitch=cameras[c].pitch;

    V fwd,right,up;
    angle_vectors(g->cam.yaw,g->cam.pitch,&fwd,&right,&up);
    g->cam.pos=sub(model_pos,scl(fwd,cameras[c].dist));
    g->cam.pos.z+=50; // Lift camera up

    // Render several frames of animation
    for(int f=0;f<3;f++){
      int frame_offset=(a->count*f)/3;
      int frame1=a->first+frame_offset;
      int frame2=a->first+(frame_offset+1)%a->count;

      glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

      // Render lower (legs) - green
      render_md3(g,&g->player.lower,frame1,frame2,0,model_pos,v3(0.2f,0.8f,0.2f));

      // Render upper (torso) - blue (offset up)
      V upper_pos=add(model_pos,v3(0,0,24));
      render_md3(g,&g->player.upper,frame1,frame2,0,upper_pos,v3(0.3f,0.5f,1.0f));

      // Render head - red (offset up more)
      V head_pos=add(model_pos,v3(0,0,48));
      render_md3(g,&g->player.head,frame1,frame2,0,head_pos,v3(1.0f,0.3f,0.3f));

      SDL_GL_SwapWindow(g->win);

      // Screenshot
      char fname[128];
      sprintf(fname,"test_%03d_%s_%s_f%d.ppm",g->screenshot_count++,anim_name,cameras[c].view,f);
      screenshot(fname,g->cam.w,g->cam.h);
    }
  }
}

int main(int argc,char**argv){
  printf("╔═══════════════════════════════════════════════════════════╗\n");
  printf("║       Quake 3 MD3 Model & Animation Test Suite          ║\n");
  printf("║  Tests character models, animations, and camera system  ║\n");
  printf("╚═══════════════════════════════════════════════════════════╝\n\n");

  G g={0};

  // Init SDL/OpenGL
  SDL_Init(SDL_INIT_VIDEO);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);

  g.cam.w=1920;
  g.cam.h=1080;
  g.win=SDL_CreateWindow("MD3 Test",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
    g.cam.w,g.cam.h,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);

  if(!g.win){
    printf("SDL_CreateWindow failed: %s\n",SDL_GetError());
    return 1;
  }

  g.ctx=SDL_GL_CreateContext(g.win);
  if(!g.ctx){
    printf("SDL_GL_CreateContext failed: %s\n",SDL_GetError());
    return 1;
  }

  glewExperimental=GL_TRUE;
  GLenum err=glewInit();
  if(err!=GLEW_OK){
    printf("glewInit failed: %s\n",glewGetErrorString(err));
    return 1;
  }

  printf("OpenGL initialized successfully\n");
  printf("GL Version: %s\n",glGetString(GL_VERSION));

  glViewport(0,0,g.cam.w,g.cam.h);
  glEnable(GL_DEPTH_TEST);
  glClearColor(0.1f,0.1f,0.15f,1);

  init_shaders(&g);

  glGenVertexArrays(1,&g.vao);
  glGenBuffers(1,&g.vbo);
  glGenBuffers(1,&g.ebo);

  // Load player model
  const char*model=argc>1?argv[1]:"sarge";
  g.player=ld_player(model);

  if(!g.player.lower.nframes){
    printf("Failed to load model\n");
    return 1;
  }

  printf("\n=== Running Test Scenarios ===\n");

  // Test various animations
  test_scenario(&g,"IDLE","Idle stance");
  test_scenario(&g,"WALK","Walking forward");
  test_scenario(&g,"RUN","Running");
  test_scenario(&g,"JUMP","Jumping");
  test_scenario(&g,"WALKCR","Crouch walk");
  test_scenario(&g,"ATTACK","Attack animation");

  printf("\n=== Test Complete ===\n");
  printf("Generated %d screenshots\n",g.screenshot_count);

  SDL_GL_DeleteContext(g.ctx);
  SDL_DestroyWindow(g.win);
  SDL_Quit();

  return 0;
}
