// Quake 3 Raytracing Clone - Single File Implementation
// (C) 2026 - C99, Code-Golf Style, Literate Programming
// Build: gcc -std=c99 -O3 -march=native q3rt.c -lSDL2 -lGL -lm -o q3rt

#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

// OpenGL 4.5 extensions
#define GL_COMPUTE_SHADER 0x91B9
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#define GL_RGBA32F 0x8814
#define GL_READ_WRITE 0x88BA

typedef unsigned(*PFN_glCreateShader)(unsigned);
typedef void(*PFN_glShaderSource)(unsigned,int,const char**,const int*);
typedef void(*PFN_glCompileShader)(unsigned);
typedef void(*PFN_glGetShaderiv)(unsigned,unsigned,int*);
typedef void(*PFN_glGetShaderInfoLog)(unsigned,int,int*,char*);
typedef unsigned(*PFN_glCreateProgram)(void);
typedef void(*PFN_glAttachShader)(unsigned,unsigned);
typedef void(*PFN_glLinkProgram)(unsigned);
typedef void(*PFN_glGetProgramiv)(unsigned,unsigned,int*);
typedef void(*PFN_glGetProgramInfoLog)(unsigned,int,int*,char*);
typedef void(*PFN_glUseProgram)(unsigned);
typedef void(*PFN_glDispatchCompute)(unsigned,unsigned,unsigned);
typedef void(*PFN_glMemoryBarrier)(unsigned);
typedef void(*PFN_glGenBuffers)(int,unsigned*);
typedef void(*PFN_glBindBuffer)(unsigned,unsigned);
typedef void(*PFN_glBufferData)(unsigned,long,const void*,unsigned);
typedef void(*PFN_glBindBufferBase)(unsigned,unsigned,unsigned);
typedef void(*PFN_glBindImageTexture)(unsigned,unsigned,int,unsigned char,int,unsigned,unsigned);
typedef void(*PFN_glGenerateMipmap)(unsigned);

PFN_glCreateShader pfnCreateShader;
PFN_glShaderSource pfnShaderSource;
PFN_glCompileShader pfnCompileShader;
PFN_glGetShaderiv pfnGetShaderiv;
PFN_glGetShaderInfoLog pfnGetShaderInfoLog;
PFN_glCreateProgram pfnCreateProgram;
PFN_glAttachShader pfnAttachShader;
PFN_glLinkProgram pfnLinkProgram;
PFN_glGetProgramiv pfnGetProgramiv;
PFN_glGetProgramInfoLog pfnGetProgramInfoLog;
PFN_glUseProgram pfnUseProgram;
PFN_glDispatchCompute pfnDispatchCompute;
PFN_glMemoryBarrier pfnMemoryBarrier;
PFN_glGenBuffers pfnGenBuffers;
PFN_glBindBuffer pfnBindBuffer;
PFN_glBufferData pfnBufferData;
PFN_glBindBufferBase pfnBindBufferBase;
PFN_glBindImageTexture pfnBindImageTexture;
PFN_glGenerateMipmap pfnGenerateMipmap;

#define W 1920
#define H 1080
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82

typedef void(*PFN_glGetTexImage)(unsigned,int,unsigned,unsigned,void*);
PFN_glGetTexImage pfnGetTexImage;

typedef struct{int o,l;}LMP;
typedef struct{int id,v;LMP l[17];}HDR;
typedef struct{float p[3],s[2],t[2],n[3];unsigned char c[4];}VTX;
typedef struct{int h,f,y,fv,nv,fi,ni,lm,lx,ly,lw,lh;float lo[3],lv[9];int pw,ph;}SRF;

SDL_Window*w;SDL_GLContext c;unsigned p,t,b[4];float m[16],cam[3]={0,100,0};int frame=0;
VTX*verts;int*idx;SRF*surf;int nv,ni,ns;unsigned char*bsp;

void*P(const char*n){return SDL_GL_GetProcAddress(n);}

void L(){
pfnCreateShader=P("glCreateShader");
pfnShaderSource=P("glShaderSource");
pfnCompileShader=P("glCompileShader");
pfnGetShaderiv=P("glGetShaderiv");
pfnGetShaderInfoLog=P("glGetShaderInfoLog");
pfnCreateProgram=P("glCreateProgram");
pfnAttachShader=P("glAttachShader");
pfnLinkProgram=P("glLinkProgram");
pfnGetProgramiv=P("glGetProgramiv");
pfnGetProgramInfoLog=P("glGetProgramInfoLog");
pfnUseProgram=P("glUseProgram");
pfnDispatchCompute=P("glDispatchCompute");
pfnMemoryBarrier=P("glMemoryBarrier");
pfnGenBuffers=P("glGenBuffers");
pfnBindBuffer=P("glBindBuffer");
pfnBufferData=P("glBufferData");
pfnBindBufferBase=P("glBindBufferBase");
pfnBindImageTexture=P("glBindImageTexture");
pfnGenerateMipmap=P("glGenerateMipmap");
pfnGetTexImage=P("glGetTexImage");
}

void T(const char*n){
FILE*f=fopen(n,"wb");
unsigned char h[18]={0,0,2,0,0,0,0,0,0,0,0,0,W&255,W>>8,H&255,H>>8,32,8};
float*d=malloc(W*H*4*sizeof(float));
glBindTexture(0x0DE1,t);pfnGetTexImage(0x0DE1,0,0x1908,0x1406,d);
fwrite(h,1,18,f);
for(int i=0;i<W*H;i++){
unsigned char r=d[i*4]*255,g=d[i*4+1]*255,b=d[i*4+2]*255,a=d[i*4+3]*255;
fputc(b,f);fputc(g,f);fputc(r,f);fputc(a,f);
}
fclose(f);free(d);printf("Screenshot: %s\n",n);
}

unsigned S(const char*s,unsigned t){
unsigned h=pfnCreateShader(t);int r,l=strlen(s);
pfnShaderSource(h,1,&s,&l);pfnCompileShader(h);pfnGetShaderiv(h,GL_COMPILE_STATUS,&r);
if(!r){char e[1024];pfnGetShaderInfoLog(h,1024,0,e);printf("Shader error:\n%s\n",e);exit(1);}
return h;
}

unsigned C(const char*s){
unsigned h=S(s,GL_COMPUTE_SHADER),pg=pfnCreateProgram();int r;
pfnAttachShader(pg,h);pfnLinkProgram(pg);pfnGetProgramiv(pg,GL_LINK_STATUS,&r);
if(!r){char e[1024];pfnGetProgramInfoLog(pg,1024,0,e);printf("Link error:\n%s\n",e);exit(1);}
return pg;
}

void B(const char*n){
FILE*f=fopen(n,"rb");if(!f){printf("Cannot open %s\n",n);exit(1);}
fseek(f,0,2);int sz=ftell(f);fseek(f,0,0);
bsp=malloc(sz);fread(bsp,1,sz,f);fclose(f);
HDR*h=(HDR*)bsp;
if(h->id!=0x50534249){printf("Bad BSP magic\n");exit(1);}
if(h->v!=0x2e){printf("Bad BSP version %d\n",h->v);exit(1);}
verts=(VTX*)(bsp+h->l[10].o);nv=h->l[10].l/sizeof(VTX);
idx=(int*)(bsp+h->l[11].o);ni=h->l[11].l/4;
surf=(SRF*)(bsp+h->l[13].o);ns=h->l[13].l/sizeof(SRF);
printf("BSP: %d verts, %d idx, %d surf\n",nv,ni,ns);
}

void I(){
SDL_Init(SDL_INIT_VIDEO);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,5);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
w=SDL_CreateWindow("Q3RT",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W,H,SDL_WINDOW_OPENGL);
c=SDL_GL_CreateContext(w);L();
glGenTextures(1,&t);glBindTexture(0x0DE1,t);
glTexImage2D(0x0DE1,0,GL_RGBA32F,W,H,0,0x1908,0x1406,0);
glTexParameteri(0x0DE1,0x2801,0x2601);
glTexParameteri(0x0DE1,0x2800,0x2601);
pfnBindImageTexture(0,t,0,0,0,GL_READ_WRITE,GL_RGBA32F);
pfnGenBuffers(4,b);
}

int main(int a,char**v){
const char*map=a>1?v[1]:"assets/maps/aggressor.bsp";
B(map);I();
p=C("#version 450\n"
"layout(local_size_x=16,local_size_y=16)in;\n"
"layout(rgba32f,binding=0)uniform image2D img;\n"
"void main(){\n"
"ivec2 p=ivec2(gl_GlobalInvocationID.xy);\n"
"vec2 uv=vec2(p)/vec2(1920,1080);\n"
"imageStore(img,p,vec4(uv,0.5,1));\n"
"}\n");
pfnUseProgram(p);
for(SDL_Event e;;){
while(SDL_PollEvent(&e))if(e.type==SDL_QUIT)goto X;
pfnDispatchCompute((W+15)/16,(H+15)/16,1);
pfnMemoryBarrier(0x00000020);
if(++frame==5){T("phase1_test.tga");goto X;}
SDL_GL_SwapWindow(w);SDL_Delay(16);
}
X:SDL_Quit();return 0;
}
