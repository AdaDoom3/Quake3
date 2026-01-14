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

// Load MD3 single frame
static V*load_md3_frame0(const char*path,int*nverts,int*ntris,int**tris){
  FILE*f=fopen(path,"rb");
  if(!f){printf("Can't open %s\n",path);return NULL;}

  // Read header
  char id[4];int ver;
  fread(id,4,1,f);
  fread(&ver,4,1,f);

  if(memcmp(id,"IDP3",4)||ver!=15){
    printf("Bad MD3\n");
    fclose(f);
    return NULL;
  }

  // Skip to mesh offset (at byte 108)
  int ofs_meshes;
  fseek(f,104,SEEK_SET);
  fread(&ofs_meshes,4,1,f);

  // Go to first mesh
  fseek(f,ofs_meshes,SEEK_SET);

  // Read mesh header
  char mesh_id[4];
  fread(mesh_id,4,1,f);
  fseek(f,ofs_meshes+68,SEEK_SET);  // Skip to nverts
  int nv,nt;
  fread(&nv,4,1,f);
  fread(&nt,4,1,f);

  printf("Mesh: %d verts, %d tris\n",nv,nt);

  // Read triangle offsets
  int ofs_tris,ofs_verts;
  fread(&ofs_tris,4,1,f);
  fseek(f,4,SEEK_CUR);  // Skip ofs_shaders
  fseek(f,4,SEEK_CUR);  // Skip ofs_st
  fread(&ofs_verts,4,1,f);

  // Read triangles
  fseek(f,ofs_meshes+ofs_tris,SEEK_SET);
  *tris=malloc(nt*3*sizeof(int));
  fread(*tris,4,nt*3,f);
  *ntris=nt;

  // Read vertices (frame 0)
  fseek(f,ofs_meshes+ofs_verts,SEEK_SET);
  short*shorts=malloc(nv*4*sizeof(short));
  fread(shorts,sizeof(short),nv*4,f);

  V*verts=malloc(nv*sizeof(V));
  for(int i=0;i<nv;i++){
    verts[i].x=shorts[i*4+0]/64.0f;
    verts[i].y=shorts[i*4+1]/64.0f;
    verts[i].z=shorts[i*4+2]/64.0f;
  }

  printf("First 3 verts: (%.1f,%.1f,%.1f) (%.1f,%.1f,%.1f) (%.1f,%.1f,%.1f)\n",
    verts[0].x,verts[0].y,verts[0].z,
    verts[1].x,verts[1].y,verts[1].z,
    verts[2].x,verts[2].y,verts[2].z);

  free(shorts);
  fclose(f);
  *nverts=nv;
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

  // Simple shaders
  const char*vs=
    "#version 330 core\n"
    "layout(location=0)in vec3 P;"
    "uniform mat4 MVP;"
    "void main(){gl_Position=MVP*vec4(P,1);}";

  const char*fs=
    "#version 330 core\n"
    "out vec4 F;"
    "void main(){F=vec4(0.3,0.8,0.3,1);}";

  unsigned int vsh=glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vsh,1,&vs,0);
  glCompileShader(vsh);

  unsigned int fsh=glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fsh,1,&fs,0);
  glCompileShader(fsh);

  unsigned int prog=glCreateProgram();
  glAttachShader(prog,vsh);
  glAttachShader(prog,fsh);
  glLinkProgram(prog);

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

  printf("\nRendering...\n");

  // Build MVP matrix
  // View: camera at (0, 0, 100) looking at origin
  // Projection: perspective
  float aspect=800.0f/600.0f;
  float fov=60.0f*PI/180.0f;
  float ftan=1.0f/tanf(fov/2.0f);

  float proj[16]={
    ftan/aspect,0,0,0,
    0,ftan,0,0,
    0,0,-1.01f,-1,
    0,0,-2.01f,0
  };

  float view[16]={
    1,0,0,0,
    0,1,0,0,
    0,0,1,0,
    0,0,-100,1
  };

  float mvp[16];
  memset(mvp,0,sizeof(mvp));
  for(int i=0;i<4;i++)
    for(int j=0;j<4;j++)
      for(int k=0;k<4;k++)
        mvp[i*4+j]+=proj[i*4+k]*view[k*4+j];

  // Render one frame
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

  glUseProgram(prog);
  glUniformMatrix4fv(glGetUniformLocation(prog,"MVP"),1,GL_FALSE,mvp);

  glBindVertexArray(vao);
  glDrawElements(GL_TRIANGLES,ntris*3,GL_UNSIGNED_INT,0);

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
