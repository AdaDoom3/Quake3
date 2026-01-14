// Minimal OpenGL test - just render a triangle
#include <stdio.h>
#include <SDL2/SDL.h>
#include <GL/glew.h>

int main(){
  SDL_Init(SDL_INIT_VIDEO);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);

  SDL_Window*win=SDL_CreateWindow("Test",0,0,800,600,SDL_WINDOW_OPENGL);
  SDL_GLContext ctx=SDL_GL_CreateContext(win);
  glewInit();

  printf("GL: %s\n",glGetString(GL_VERSION));

  glViewport(0,0,800,600);
  glClearColor(0.2f,0.2f,0.3f,1);

  const char*vs=
    "#version 330 core\n"
    "layout(location=0)in vec2 P;"
    "void main(){gl_Position=vec4(P,0,1);}";

  const char*fs=
    "#version 330 core\n"
    "out vec4 F;"
    "void main(){F=vec4(1,0,0,1);}";

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

  // Triangle in NDC space
  float verts[]={0.0f,0.5f,-0.5f,-0.5f,0.5f,-0.5f};

  unsigned int vao,vbo;
  glGenVertexArrays(1,&vao);
  glBindVertexArray(vao);
  glGenBuffers(1,&vbo);
  glBindBuffer(GL_ARRAY_BUFFER,vbo);
  glBufferData(GL_ARRAY_BUFFER,sizeof(verts),verts,GL_STATIC_DRAW);
  glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,0);
  glEnableVertexAttribArray(0);

  glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(prog);
  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLES,0,3);
  SDL_GL_SwapWindow(win);

  // Screenshot
  unsigned char*pix=malloc(800*600*3);
  glReadPixels(0,0,800,600,GL_RGB,GL_UNSIGNED_BYTE,pix);
  FILE*f=fopen("gltest.ppm","wb");
  fprintf(f,"P6\n800 600\n255\n");
  for(int y=599;y>=0;y--)fwrite(pix+y*800*3,1,800*3,f);
  fclose(f);
  free(pix);

  printf("Screenshot: gltest.ppm\n");

  SDL_Delay(100);
  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
