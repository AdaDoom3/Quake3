// Simple MD3 viewer to debug rendering
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <GL/glew.h>

#define PI 3.14159265359f

typedef struct{float x,y,z;}V;

static V v3(float x,float y,float z){return (V){x,y,z};}

// MD3 structures
typedef struct{char id[4];int ver;char name[64];int flags;int nframes,ntags,nmeshes,nskins;int ofs_frames,ofs_tags,ofs_meshes,ofs_eof;}MD3H;
typedef struct{char id[4];char name[64];int flags;int nframes,nshaders,nverts,ntris;int ofs_tris,ofs_shaders,ofs_st,ofs_verts,ofs_end;}MD3M;

// Load MD3 single frame using working approach from q3.c
static V*load_md3_frame0(const char*path,int*nverts,int*ntris,int**tris){
  FILE*f=fopen(path,"rb");
  if(!f){printf("Can't open %s\n",path);return NULL;}

  // Get file size
  fseek(f,0,SEEK_END);
  int sz=ftell(f);
  fseek(f,0,SEEK_SET);

  // Read entire file
  unsigned char*d=malloc(sz);
  fread(d,1,sz,f);
  fclose(f);

  MD3H*h=(MD3H*)d;
  if(memcmp(h->id,"IDP3",4)||h->ver!=15){
    printf("Bad MD3\n");
    free(d);
    return NULL;
  }

  // Get first mesh
  MD3M*mesh=(MD3M*)(d+h->ofs_meshes);
  unsigned char*mesh_base=(unsigned char*)mesh;

  printf("Mesh: %d verts, %d tris, %d frames\n",mesh->nverts,mesh->ntris,mesh->nframes);

  // Read vertices for frame 0
  short*vdata=(short*)(mesh_base+mesh->ofs_verts);
  int nv=mesh->nverts;

  V*verts=malloc(nv*sizeof(V));
  for(int i=0;i<nv;i++){
    verts[i].x=vdata[i*4+0]/64.0f;
    verts[i].y=vdata[i*4+1]/64.0f;
    verts[i].z=vdata[i*4+2]/64.0f;
  }

  printf("First 3 verts: (%.1f,%.1f,%.1f) (%.1f,%.1f,%.1f) (%.1f,%.1f,%.1f)\n",
    verts[0].x,verts[0].y,verts[0].z,
    verts[1].x,verts[1].y,verts[1].z,
    verts[2].x,verts[2].y,verts[2].z);

  // Read triangle indices
  int*idx=(int*)(mesh_base+mesh->ofs_tris);
  int nt=mesh->ntris;
  *tris=malloc(nt*3*sizeof(int));
  memcpy(*tris,idx,nt*3*sizeof(int));

  free(d);
  *nverts=nv;
  *ntris=nt;
  return verts;
}

int main(){
  printf("=== Simple MD3 Viewer ===\n\n");

  // Load model
  int nverts,ntris,*tris;
  V*verts=load_md3_frame0("assets/models/players/sarge/lower.md3",&nverts,&ntris,&tris);

  if(!verts){
    printf("Failed to load model\n");
    return 1;
  }

  // Init SDL/OpenGL
  SDL_Init(SDL_INIT_VIDEO);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);

  SDL_Window*win=SDL_CreateWindow("MD3",0,0,800,600,SDL_WINDOW_OPENGL);
  SDL_GLContext ctx=SDL_GL_CreateContext(win);

  glewInit();
  printf("GL: %s\n",glGetString(GL_VERSION));

  glViewport(0,0,800,600);
  glClearColor(0.2f,0.2f,0.3f,1);
  glEnable(GL_DEPTH_TEST);

  // Simple shaders - just display the model scaled to NDC
  const char*vs=
    "#version 330 core\n"
    "layout(location=0)in vec3 P;"
    "void main(){"
    "  vec3 scaled=P*0.03;"  // Scale to fit in view
    "  gl_Position=vec4(scaled,1);"  // Direct to NDC
    "}";

  const char*fs=
    "#version 330 core\n"
    "out vec4 F;"
    "void main(){F=vec4(0.3,0.9,0.3,1);}";

  unsigned int vsh=glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vsh,1,&vs,0);
  glCompileShader(vsh);
  int ok;
  glGetShaderiv(vsh,GL_COMPILE_STATUS,&ok);
  if(!ok){
    char log[512];
    glGetShaderInfoLog(vsh,512,0,log);
    printf("VS error: %s\n",log);
  }

  unsigned int fsh=glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fsh,1,&fs,0);
  glCompileShader(fsh);
  glGetShaderiv(fsh,GL_COMPILE_STATUS,&ok);
  if(!ok){
    char log[512];
    glGetShaderInfoLog(fsh,512,0,log);
    printf("FS error: %s\n",log);
  }

  unsigned int prog=glCreateProgram();
  glAttachShader(prog,vsh);
  glAttachShader(prog,fsh);
  glLinkProgram(prog);
  glGetProgramiv(prog,GL_LINK_STATUS,&ok);
  if(!ok){
    char log[512];
    glGetProgramInfoLog(prog,512,0,log);
    printf("Link error: %s\n",log);
  }
  printf("Shaders compiled and linked successfully\n");

  // Upload geometry
  unsigned int vao,vbo,ebo;
  glGenVertexArrays(1,&vao);
  glBindVertexArray(vao);

  glGenBuffers(1,&vbo);
  glBindBuffer(GL_ARRAY_BUFFER,vbo);
  glBufferData(GL_ARRAY_BUFFER,nverts*sizeof(V),verts,GL_STATIC_DRAW);
  glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,0);
  glEnableVertexAttribArray(0);

  glGenBuffers(1,&ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,ntris*3*sizeof(int),tris,GL_STATIC_DRAW);

  printf("\nRendering with simple NDC transform...\n");

  // Render one frame
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  // Draw as solid with no culling
  glDisable(GL_CULL_FACE);

  glUseProgram(prog);
  glBindVertexArray(vao);
  glDrawElements(GL_TRIANGLES,ntris*3,GL_UNSIGNED_INT,0);

  GLenum err=glGetError();
  if(err!=GL_NO_ERROR)printf("GL Error: 0x%x\n",err);

  SDL_GL_SwapWindow(win);

  // Screenshot
  unsigned char*pix=malloc(800*600*3);
  glReadPixels(0,0,800,600,GL_RGB,GL_UNSIGNED_BYTE,pix);

  FILE*f=fopen("simple_test.ppm","wb");
  fprintf(f,"P6\n800 600\n255\n");
  for(int y=599;y>=0;y--)fwrite(pix+y*800*3,1,800*3,f);
  fclose(f);
  free(pix);

  printf("Screenshot: simple_test.ppm\n");

  SDL_Delay(100);

  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(win);
  SDL_Quit();

  free(verts);
  free(tris);

  return 0;
}
