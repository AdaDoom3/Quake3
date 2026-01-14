// Quake 3 idtech3 Clone - Single Translation Unit
// Principles: extreme consolidation, shader primacy, economy of expression
// Tools: C99, SDL2, OpenGL 3.3+ core

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>

typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;typedef uint64_t u64;
typedef int32_t i32;typedef float f32;typedef double f64;

SDL_Window*W;SDL_GLContext C;u32 w=1280,h=720;f64 t,dt;u32 running=1,frame=0,maxframes=0;

typedef struct{i32 off,len;}Lump;
typedef struct{char magic[4];i32 ver;Lump lumps[17];}BspHdr;
typedef struct{f32 min[3],max[3];i32 face,nfaces,leaf,nleafs;}BspModel;
typedef struct{char name[64];i32 flags,contents;}BspTex;
typedef struct{f32 n[3];f32 d;}BspPlane;
typedef struct{i32 plane,children[2],min[3],max[3];}BspNode;
typedef struct{i32 cluster,area,min[3],max[3],face,nfaces,brush,nbrushes;}BspLeaf;
typedef struct{i32 tex,effect,type,vert,nverts,idx,nidxs,lm,lmcorner[2],lmsize[2],lmorigin[3],lmvecs[2][3],n[3],size[2];}BspFace;
typedef struct{f32 pos[3],uv[2],lmuv[2];u8 n[3];u8 col[4];}BspVert;

typedef struct{u32 w,h;u8*d;}Tex;

u8*bsp;BspHdr*bsph;BspTex*bsptex;BspFace*bspface;BspVert*bspvert;i32*bspidx;u8*bsplm;
i32 ntex,nface,nvert,nidx;
u32 worldProg,worldVAO,worldVBO,worldIBO,dummyTex,dummyLM;
u32*texIds;
f32 camPos[3]={0,50,200},camAng[2]={0.0f,3.14f};
f32 camSpeed=200.0f;

void die(const char*s){fprintf(stderr,"FATAL: %s\n",s);exit(1);}

void perspective(f32*m,f32 fov,f32 asp,f32 n,f32 f){
memset(m,0,16*sizeof(f32));f32 t=tanf(fov*0.5f);
m[0]=1.0f/(asp*t);m[5]=1.0f/t;m[10]=-(f+n)/(f-n);m[11]=-1.0f;m[14]=-(2.0f*f*n)/(f-n);
}

void lookAt(f32*m,f32*e,f32*c,f32*u){
f32 f[3]={c[0]-e[0],c[1]-e[1],c[2]-e[2]},l=sqrtf(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]);
f[0]/=l;f[1]/=l;f[2]/=l;
f32 s[3]={f[1]*u[2]-f[2]*u[1],f[2]*u[0]-f[0]*u[2],f[0]*u[1]-f[1]*u[0]};
l=sqrtf(s[0]*s[0]+s[1]*s[1]+s[2]*s[2]);s[0]/=l;s[1]/=l;s[2]/=l;
f32 up[3]={s[1]*f[2]-s[2]*f[1],s[2]*f[0]-s[0]*f[2],s[0]*f[1]-s[1]*f[0]};
memset(m,0,16*sizeof(f32));
m[0]=s[0];m[4]=s[1];m[8]=s[2];m[1]=up[0];m[5]=up[1];m[9]=up[2];
m[2]=-f[0];m[6]=-f[1];m[10]=-f[2];m[15]=1.0f;
m[12]=-(s[0]*e[0]+s[1]*e[1]+s[2]*e[2]);
m[13]=-(up[0]*e[0]+up[1]*e[1]+up[2]*e[2]);
m[14]=f[0]*e[0]+f[1]*e[1]+f[2]*e[2];
}

char*slurp(const char*p,size_t*sz){
FILE*f=fopen(p,"rb");if(!f)return NULL;
fseek(f,0,SEEK_END);size_t n=ftell(f);fseek(f,0,SEEK_SET);
char*d=malloc(n+1);if(!d){fclose(f);return NULL;}
fread(d,1,n,f);d[n]=0;fclose(f);if(sz)*sz=n;return d;
}

Tex loadTGA(const char*p){
Tex t={0};u8 hdr[18];FILE*f=fopen(p,"rb");if(!f)return t;
fread(hdr,1,18,f);
u32 w=hdr[12]|(hdr[13]<<8),h=hdr[14]|(hdr[15]<<8),bpp=hdr[16];
if(bpp!=24&&bpp!=32){fclose(f);return t;}
u32 sz=w*h*(bpp/8);t.w=w;t.h=h;t.d=malloc(w*h*4);
u8*tmp=malloc(sz);fread(tmp,1,sz,f);fclose(f);
for(u32 i=0;i<w*h;i++){
t.d[i*4+0]=tmp[i*(bpp/8)+2];t.d[i*4+1]=tmp[i*(bpp/8)+1];
t.d[i*4+2]=tmp[i*(bpp/8)+0];t.d[i*4+3]=bpp==32?tmp[i*4+3]:255;
}
free(tmp);return t;
}

void loadBSP(const char*p){
size_t sz;bsp=(u8*)slurp(p,&sz);if(!bsp)die("BSP not found");
bsph=(BspHdr*)bsp;
if(memcmp(bsph->magic,"IBSP",4)||bsph->ver!=0x2e)die("Invalid BSP");
bsptex=(BspTex*)(bsp+bsph->lumps[1].off);ntex=bsph->lumps[1].len/sizeof(BspTex);
bspface=(BspFace*)(bsp+bsph->lumps[13].off);nface=bsph->lumps[13].len/sizeof(BspFace);
bspvert=(BspVert*)(bsp+bsph->lumps[10].off);nvert=bsph->lumps[10].len/sizeof(BspVert);
bspidx=(i32*)(bsp+bsph->lumps[11].off);nidx=bsph->lumps[11].len/sizeof(i32);
bsplm=bsp+bsph->lumps[14].off;
printf("BSP: %d tex, %d face, %d vert, %d idx\n",ntex,nface,nvert,nidx);
}

u32 compileShader(u32 type,const char*src){
u32 s=glCreateShader(type);glShaderSource(s,1,&src,NULL);glCompileShader(s);
i32 ok;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
if(!ok){char log[512];glGetShaderInfoLog(s,512,NULL,log);
fprintf(stderr,"Shader error: %s\n",log);return 0;}
return s;
}

u32 linkProgram(u32 vs,u32 fs){
u32 p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);
glLinkProgram(p);i32 ok;glGetProgramiv(p,GL_LINK_STATUS,&ok);
if(!ok){char log[512];glGetProgramInfoLog(p,512,NULL,log);
fprintf(stderr,"Link error: %s\n",log);return 0;}
return p;
}

u32 loadShader(const char*vp,const char*fp){
size_t vsz,fsz;char*vs=slurp(vp,&vsz),*fs=slurp(fp,&fsz);
if(!vs||!fs)die("Failed to load shaders");
u32 v=compileShader(GL_VERTEX_SHADER,vs);
u32 f=compileShader(GL_FRAGMENT_SHADER,fs);
free(vs);free(fs);if(!v||!f)die("Shader compile failed");
u32 p=linkProgram(v,f);glDeleteShader(v);glDeleteShader(f);
if(!p)die("Shader link failed");
return p;
}

u32 makeTex(u32 w,u32 h,u8*d){
u32 t;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);
glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,d);
glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
return t;
}

void loadTextures(){
printf("Loading %d textures...\n",ntex);
texIds=malloc(ntex*sizeof(u32));u8 white[4]={255,255,255,255};
dummyTex=makeTex(1,1,white);i32 loaded=0;
for(i32 i=0;i<ntex;i++){
char path[256];snprintf(path,256,"assets/%s.tga",bsptex[i].name);
Tex t=loadTGA(path);
if(t.d){texIds[i]=makeTex(t.w,t.h,t.d);free(t.d);loaded++;}
else texIds[i]=dummyTex;
}
printf("Loaded %d/%d textures\n",loaded,ntex);
}

void initWorld(){
loadTextures();
worldProg=loadShader("world.vs","world.fs");
glGenVertexArrays(1,&worldVAO);glBindVertexArray(worldVAO);
glGenBuffers(1,&worldVBO);glBindBuffer(GL_ARRAY_BUFFER,worldVBO);
glBufferData(GL_ARRAY_BUFFER,nvert*sizeof(BspVert),bspvert,GL_STATIC_DRAW);
glGenBuffers(1,&worldIBO);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,worldIBO);
glBufferData(GL_ELEMENT_ARRAY_BUFFER,nidx*sizeof(i32),bspidx,GL_STATIC_DRAW);
glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,0,sizeof(BspVert),(void*)0);
glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,0,sizeof(BspVert),(void*)12);
glEnableVertexAttribArray(2);glVertexAttribPointer(2,2,GL_FLOAT,0,sizeof(BspVert),(void*)20);
glEnableVertexAttribArray(3);glVertexAttribPointer(3,3,GL_UNSIGNED_BYTE,1,sizeof(BspVert),(void*)28);
glEnableVertexAttribArray(4);glVertexAttribPointer(4,4,GL_UNSIGNED_BYTE,1,sizeof(BspVert),(void*)31);
glBindVertexArray(0);
u8 white2[4]={255,255,255,255};dummyLM=makeTex(1,1,white2);
printf("World GPU upload complete\n");
}

void initGL(){
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
W=SDL_CreateWindow("Q3",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,w,h,
SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
if(!W)die("Window creation failed");
C=SDL_GL_CreateContext(W);if(!C)die("GL context failed");
if(glewInit()!=GLEW_OK)die("GLEW init failed");
SDL_GL_SetSwapInterval(1);
glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);
glEnable(GL_CULL_FACE);glCullFace(GL_BACK);
printf("OpenGL: %s\n",glGetString(GL_VERSION));
}

void handleEvents(){
SDL_Event e;
while(SDL_PollEvent(&e)){
if(e.type==SDL_QUIT)running=0;
if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_ESCAPE)running=0;
}
const u8*k=SDL_GetKeyboardState(NULL);
f32 spd=camSpeed*(f32)dt;
f32 fwd[3]={cosf(camAng[1])*cosf(camAng[0]),sinf(camAng[0]),sinf(camAng[1])*cosf(camAng[0])};
f32 rgt[3]={cosf(camAng[1]+1.57f),0,sinf(camAng[1]+1.57f)};
if(k[SDL_SCANCODE_W]){camPos[0]+=fwd[0]*spd;camPos[1]+=fwd[1]*spd;camPos[2]+=fwd[2]*spd;}
if(k[SDL_SCANCODE_S]){camPos[0]-=fwd[0]*spd;camPos[1]-=fwd[1]*spd;camPos[2]-=fwd[2]*spd;}
if(k[SDL_SCANCODE_A]){camPos[0]-=rgt[0]*spd;camPos[2]-=rgt[2]*spd;}
if(k[SDL_SCANCODE_D]){camPos[0]+=rgt[0]*spd;camPos[2]+=rgt[2]*spd;}
if(k[SDL_SCANCODE_SPACE])camPos[1]+=spd;
if(k[SDL_SCANCODE_LSHIFT])camPos[1]-=spd;
if(k[SDL_SCANCODE_LEFT])camAng[1]-=2.0f*(f32)dt;
if(k[SDL_SCANCODE_RIGHT])camAng[1]+=2.0f*(f32)dt;
if(k[SDL_SCANCODE_UP])camAng[0]+=2.0f*(f32)dt;
if(k[SDL_SCANCODE_DOWN])camAng[0]-=2.0f*(f32)dt;
if(camAng[0]>1.5f)camAng[0]=1.5f;if(camAng[0]<-1.5f)camAng[0]=-1.5f;
}

void screenshot(const char*path){
u8*pix=malloc(w*h*3);glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pix);
FILE*f=fopen(path,"wb");if(!f){free(pix);return;}
fprintf(f,"P6\n%d %d\n255\n",w,h);
for(i32 y=h-1;y>=0;y--)fwrite(pix+y*w*3,1,w*3,f);
fclose(f);free(pix);printf("Screenshot: %s\n",path);
}

void renderWorld(){
f32 proj[16],view[16];
perspective(proj,1.57f,(f32)w/h,0.1f,10000.0f);
f32 tgt[3]={camPos[0]+cosf(camAng[1])*cosf(camAng[0]),
camPos[1]+sinf(camAng[0]),
camPos[2]+sinf(camAng[1])*cosf(camAng[0])};
f32 up[3]={0,1,0};
lookAt(view,camPos,tgt,up);
glUseProgram(worldProg);
glUniformMatrix4fv(glGetUniformLocation(worldProg,"uProj"),1,0,proj);
glUniformMatrix4fv(glGetUniformLocation(worldProg,"uView"),1,0,view);
glUniform1i(glGetUniformLocation(worldProg,"uTex"),0);
glUniform1i(glGetUniformLocation(worldProg,"uLM"),1);
glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,dummyLM);
glBindVertexArray(worldVAO);
for(i32 i=0;i<nface;i++){
BspFace*f=&bspface[i];
if(f->type==1||f->type==3){
u32 tid=f->tex>=0&&f->tex<ntex?texIds[f->tex]:dummyTex;
glUniform1i(glGetUniformLocation(worldProg,"uHasTex"),tid!=dummyTex);
glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,tid);
glDrawElements(GL_TRIANGLES,f->nidxs,GL_UNSIGNED_INT,(void*)(f->idx*4));
}
}
glBindVertexArray(0);
}

void render(){
glClearColor(0.1f,0.1f,0.15f,1.0f);
glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
renderWorld();
SDL_GL_SwapWindow(W);
}

int main(int argc,char**argv){
if(argc>1)maxframes=atoi(argv[1]);
if(SDL_Init(SDL_INIT_VIDEO)<0)die("SDL init failed");
initGL();
loadBSP("assets/maps/oa_dm1.bsp");
initWorld();
u64 freq=SDL_GetPerformanceFrequency(),now=SDL_GetPerformanceCounter(),last=now;
printf("Rendering oa_dm1. ESC to quit.\n");
while(running){
now=SDL_GetPerformanceCounter();dt=(now-last)/(f64)freq;last=now;t+=dt;
handleEvents();
render();
if(frame==0)screenshot("phase4_f000.ppm");
if(frame==60)screenshot("phase4_f060.ppm");
if(frame==120)screenshot("phase4_f120.ppm");
frame++;if(maxframes&&frame>=maxframes)running=0;
}
SDL_GL_DeleteContext(C);SDL_DestroyWindow(W);SDL_Quit();
return 0;
}
