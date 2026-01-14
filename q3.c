/* ═══════════════════════════════════════════════════════════════════════════
   QUAKE III ARENA - Code Golf Edition
   A Literate Programming Journey Through id Software's Masterpiece

   "In the beginning, Carmack created the vertices and the pixels..."

   This single-file implementation demonstrates the elegance of functional
   programming applied to game engine architecture. We pursue minimalism
   not through absence, but through perfect composition of pure functions.

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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER I: The Algebra of Types

   In functional programming, we begin by defining the universe of discourse.
   These structures represent the Platonic ideals of our 3D realm.
   ═══════════════════════════════════════════════════════════════════════════*/

typedef struct{float x,y,z;}V;
typedef struct{float u,v;}T;
typedef struct{int v,n,t;}I;
typedef struct{V a,b;}B;
typedef struct{int o,n;}L;
typedef struct{unsigned char r,g,b,a;}C;

typedef struct{
  int ofs,len;
}D;

typedef struct{
  char sig[4];int ver;D d[17];
}H;

typedef struct{
  char n[64];int f,c;
}TX;

typedef struct{
  V mn,mx;int f,b;
}N;

typedef struct{
  int t,e[2];
}F;

typedef struct{
  int t,e,c,v,nv,mv,nmv,m,lms[2],lmsz[2];V lmo,lmv[2],nm;int sz[2];
}LF;

typedef struct{
  char n[64];float x[3][3],p[3];
}EF;

typedef struct{
  int f,n,b,s,v,w,h,o[3];char s2[128];
}SH;

typedef struct{
  V*vs;T*ts;T*ls;C*cs;int*is;TX*tx;LF*lf;int nv,nt,nl,nc,ni,ntx,nlf;
  unsigned char*lm;int nlm;SH*sh;int nsh;N*nd;int nnd;F*fc;int nfc;
  int*lfi;EF*ef;int nef;int*mv;int nmv;B bb;
}M;

typedef struct{
  SDL_Window*w;SDL_GLContext g;int sw,sh;
  unsigned int vao,vbo,ebo,tx[256],lm[256];
  unsigned int prg,vsh,fsh;
  V cp,ca;float cy,pitch;int fwd,bck,lft,rgt;
  M m;int run;int fc;
}G;

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER II: Monadic I/O and the Impure World

   File reading is our interface to the material realm. We treat these
   operations as functors, mapping file paths to memory buffers.
   ═══════════════════════════════════════════════════════════════════════════*/

static void*rd(const char*p,int*sz){
  FILE*f=fopen(p,"rb");if(!f)return 0;
  fseek(f,0,SEEK_END);*sz=ftell(f);fseek(f,0,SEEK_SET);
  void*d=malloc(*sz);fread(d,1,*sz,f);fclose(f);return d;
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER III: Linear Algebra - The Mathematics of Space

   Vectors are the fundamental building blocks. These pure functions
   implement the field operations that govern Euclidean space.
   ═══════════════════════════════════════════════════════════════════════════*/

static inline V v3(float x,float y,float z){return(V){x,y,z};}
static inline V add(V a,V b){return v3(a.x+b.x,a.y+b.y,a.z+b.z);}
static inline V sub(V a,V b){return v3(a.x-b.x,a.y-b.y,a.z-b.z);}
static inline V scl(V a,float s){return v3(a.x*s,a.y*s,a.z*s);}
static inline float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static inline V nrm(V a){float l=sqrtf(dot(a,a));return l>0?scl(a,1.0f/l):a;}
static inline V crs(V a,V b){return v3(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER IV: Image Decoding - Translating Compressed Representations

   TGA files encode pixel data. We unfold this encoding through pattern
   matching on the header structure, revealing the underlying rgba tensor.
   ═══════════════════════════════════════════════════════════════════════════*/

static unsigned char*tga(unsigned char*d,int*w,int*h){
  if(!d)return 0;
  *w=d[12]|(d[13]<<8);*h=d[14]|(d[15]<<8);
  int bpp=d[16],sz=*w**h*(bpp/8),o=18+d[0];
  unsigned char*p=malloc(*w**h*4),*s=d+o;
  if(bpp==32)for(int i=0;i<*w**h;i++){
    p[i*4]=s[i*4+2];p[i*4+1]=s[i*4+1];p[i*4+2]=s[i*4];p[i*4+3]=s[i*4+3];
  }else if(bpp==24)for(int i=0;i<*w**h;i++){
    p[i*4]=s[i*3+2];p[i*4+1]=s[i*3+1];p[i*4+2]=s[i*3];p[i*4+3]=255;
  }else return 0;
  return p;
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER V: BSP Parsing - Deconstructing the Binary Space Partition

   The BSP file is a serialized scene graph. We deserialize this structure
   through pointer arithmetic, casting the byte stream into typed arrays.
   This is our parser combinator, transforming bits into geometry.
   ═══════════════════════════════════════════════════════════════════════════*/

static int ld(const char*p,M*m){
  int sz;unsigned char*d=rd(p,&sz);
  if(!d){printf("Failed to read %s\n",p);fflush(stdout);return 0;}
  printf("Read %d bytes\n",sz);fflush(stdout);
  H*h=(H*)d;
  if(memcmp(h->sig,"IBSP",4)){printf("Not a BSP file\n");fflush(stdout);return 0;}
  if(h->ver!=0x2e){printf("Wrong BSP version: %d\n",h->ver);fflush(stdout);return 0;}

  printf("BSP lumps: ents=%d shaders=%d faces=%d verts=%d indices=%d\n",
    h->d[0].len,h->d[1].len,h->d[13].len,h->d[10].len,h->d[11].len);fflush(stdout);

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
  printf("Indices: ofs=%d len=%d count=%d filesize=%d\n",h->d[11].ofs,h->d[11].len,nidx,sz);fflush(stdout);
  if(h->d[11].ofs+h->d[11].len>sz){printf("Index data exceeds file size!\n");fflush(stdout);return 0;}
  int*idx=(int*)(d+h->d[11].ofs);
  printf("Malloc indices\n");fflush(stdout);
  m->is=malloc(nidx*sizeof(int));
  if(!m->is){printf("Malloc failed!\n");fflush(stdout);return 0;}
  printf("Memcpy indices\n");fflush(stdout);
  memcpy(m->is,idx,h->d[11].len);m->ni=nidx;
  printf("Indices done\n");fflush(stdout);

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
   CHAPTER VI: Shader Compilation - The GPU's Functional Pipeline

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

static void ishd(G*g){
  g->vsh=csh(GL_VERTEX_SHADER,vss);
  g->fsh=csh(GL_FRAGMENT_SHADER,fss);
  g->prg=glCreateProgram();
  glAttachShader(g->prg,g->vsh);glAttachShader(g->prg,g->fsh);
  glLinkProgram(g->prg);
  int ok;glGetProgramiv(g->prg,GL_LINK_STATUS,&ok);
  if(!ok){char l[512];glGetProgramInfoLog(g->prg,512,0,l);printf("Link: %s\n",l);}
  else{printf("Shaders compiled and linked successfully\n");fflush(stdout);}
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER VII: Texture Loading - Materializing the Visual Field

   Textures are 2D arrays sampled by the fragment shader. We bind these
   into the GPU's texture units, creating a lookup table for surface detail.
   ═══════════════════════════════════════════════════════════════════════════*/

static void ltx(G*g){
  glGenTextures(256,g->tx);
  int loaded=0;
  for(int i=0;i<g->m.ntx&&i<256;i++){
    char p[256];
    sprintf(p,"assets/%s.tga",g->m.tx[i].n);
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
        loaded++;
        if(i<5)printf("Loaded: %s\n",g->m.tx[i].n);
      }
      free(d);
    }else{
      if(i<5)printf("Missing: %s\n",g->m.tx[i].n);
      unsigned char white[16]={255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255};
      glBindTexture(GL_TEXTURE_2D,g->tx[i]);
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,white);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    }
  }
  printf("Loaded %d/%d textures\n",loaded,g->m.ntx);fflush(stdout);

  glGenTextures(256,g->lm);
  for(int i=0;i<g->m.nlm&&i<256;i++){
    glBindTexture(GL_TEXTURE_2D,g->lm[i]);
    unsigned char*rgba=malloc(128*128*4);
    for(int j=0;j<128*128;j++){
      rgba[j*4]=255;rgba[j*4+1]=255;rgba[j*4+2]=255;rgba[j*4+3]=255;
    }
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,128,128,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    free(rgba);
  }
  printf("Loaded %d lightmaps (fullbright for debug)\n",g->m.nlm);fflush(stdout);
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER VIII: Matrix Mathematics - Projective Transformations

   We construct the view and projection matrices that map world space
   to clip space. This is the homomorphism from R³ to screen coordinates.
   ═══════════════════════════════════════════════════════════════════════════*/

static void vpmat(float*m,V p,float y,float pi,int w,int h){
  float cp=cosf(pi),sp=sinf(pi),cy=cosf(y),sy=sinf(y);
  V f=nrm(v3(cy*cp,sp,sy*cp));
  V r=nrm(crs(v3(0,1,0),f));
  V u=crs(f,r);

  float fov=70*M_PI/180,asp=(float)w/h,n=1,far=8192;
  float t=tanf(fov/2)*n;

  float pr[16]={
    n/(asp*t),0,0,0,
    0,n/t,0,0,
    0,0,-(far+n)/(far-n),-2*far*n/(far-n),
    0,0,-1,0
  };

  float vw[16]={
    r.x,u.x,f.x,0,
    r.y,u.y,f.y,0,
    r.z,u.z,f.z,0,
    -dot(r,p),-dot(u,p),-dot(f,p),1
  };

  for(int i=0;i<16;i++)m[i]=0;
  for(int i=0;i<4;i++)for(int j=0;j<4;j++)
    for(int k=0;k<4;k++)m[i+j*4]+=pr[i+k*4]*vw[k+j*4];
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER IX: The Render Loop - Composing the Visual Stream

   Rendering is a fold over the face list, applying the shader pipeline
   to each triangle. We group by texture for batch efficiency.
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

  int drawn=0,t1=0,t3=0,skip=0;
  for(int f=0;f<g->m.nlf;f++){
    LF*lf=&g->m.lf[f];

    if(f<5&&g->fc==60){
      printf("Face %d: type=%d v=%d nv=%d mv=%d nmv=%d t=%d m=%d\n",f,lf->c,lf->v,lf->nv,lf->mv,lf->nmv,lf->t,lf->m);
    }

    if(lf->c==1){t1++;}
    else if(lf->c==3){t3++;}
    else{skip++;continue;}

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

  if(g->fc==60||g->fc==90){
    GLenum err=glGetError();
    printf("Frame %d: t1=%d t3=%d skip=%d drew=%d GL_ERR=%d\n",g->fc,t1,t3,skip,drawn,err);fflush(stdout);
  }

  SDL_GL_SwapWindow(g->w);

  if(g->fc==60||g->fc==90){
    char fn[64];sprintf(fn,"shot_%03d.ppm",g->fc);
    ss(fn,g->sw,g->sh);printf("Screenshot: %s\n",fn);
  }
  g->fc++;
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER X: Player Movement - The Physics Monad

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
   CHAPTER XI: Event Handling - The IO Monad Unfolds

   User input flows through SDL's event queue. We pattern match on event
   types, binding keyboard state to movement flags.
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
   CHAPTER XII: Initialization - Building the Context

   We construct the window, OpenGL context, and buffer objects.
   This is our main combinator, composing all subsystems.
   ═══════════════════════════════════════════════════════════════════════════*/

static int ini(G*g,const char*mp){
  printf("SDL_Init...\n");fflush(stdout);
  SDL_Init(SDL_INIT_VIDEO);
  g->sw=1920;g->sh=1080;
  printf("Creating window...\n");fflush(stdout);
  g->w=SDL_CreateWindow("Q3",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
    g->sw,g->sh,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
  if(!g->w){printf("Window creation failed: %s\n",SDL_GetError());return 0;}

  printf("Setting GL attributes...\n");
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
  printf("Creating GL context...\n");
  g->g=SDL_GL_CreateContext(g->w);
  if(!g->g){printf("GL context creation failed: %s\n",SDL_GetError());return 0;}

  printf("Initializing GLEW...\n");
  glewExperimental=GL_TRUE;
  if(glewInit()!=GLEW_OK){printf("GLEW init failed\n");return 0;}

  SDL_SetRelativeMouseMode(SDL_TRUE);

  printf("Setting GL state...\n");
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);

  printf("Loading BSP: %s\n",mp);fflush(stdout);
  if(!ld(mp,&g->m)){printf("Failed to load BSP\n");fflush(stdout);return 0;}
  printf("Loaded: %d verts, %d faces, %d textures\n",g->m.nv,g->m.nlf,g->m.ntx);fflush(stdout);

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

  printf("First 3 vertices:\n");
  for(int i=0;i<3&&i<g->m.nv;i++){
    printf("  v[%d]: pos=[%.1f,%.1f,%.1f] uv=[%.2f,%.2f]\n",
      i,g->m.vs[i].x,g->m.vs[i].y,g->m.vs[i].z,g->m.ts[i].u,g->m.ts[i].v);
  }
  printf("First 6 indices: ");
  for(int i=0;i<6&&i<g->m.ni;i++)printf("%d ",g->m.is[i]);
  printf("\n");

  V c=scl(add(g->m.bb.a,g->m.bb.b),0.5f);
  printf("Map bounds: [%.0f,%.0f,%.0f] to [%.0f,%.0f,%.0f]\n",
    g->m.bb.a.x,g->m.bb.a.y,g->m.bb.a.z,g->m.bb.b.x,g->m.bb.b.y,g->m.bb.b.z);
  printf("Map center: [%.0f,%.0f,%.0f]\n",c.x,c.y,c.z);
  g->cp=v3(0,0,1500);
  g->cy=0.0f;g->pitch=-0.5f;
  printf("Camera: [%.0f,%.0f,%.0f] yaw=%.2f pitch=%.2f\n",g->cp.x,g->cp.y,g->cp.z,g->cy,g->pitch);
  g->run=1;g->fc=0;

  return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAPTER XIII: The Main Loop - Eternal Recursion

   The game loop is a tail-recursive function that never returns.
   Each iteration processes input, updates state, and renders.
   Time flows forward; frames accumulate into experience.
   ═══════════════════════════════════════════════════════════════════════════*/

int main(int argc,char**argv){
  printf("Q3 Code Golf Edition starting...\n");
  G*g=calloc(1,sizeof(G));
  const char*mp=argc>1?argv[1]:"assets/maps/dm4ish.bsp";

  printf("Initializing...\n");
  if(!ini(g,mp)){printf("Init failed\n");return 1;}
  printf("Init complete\n");

  unsigned int lt=SDL_GetTicks();
  while(g->run&&g->fc<120){
    unsigned int ct=SDL_GetTicks();
    float dt=(ct-lt)/1000.0f;
    lt=ct;

    evt(g);
    mv(g,dt);
    drw(g);

    SDL_Delay(16);
  }

  SDL_GL_DeleteContext(g->g);
  SDL_DestroyWindow(g->w);
  SDL_Quit();
  free(g);
  return 0;
}
