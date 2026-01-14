/*Q3VK:Literate Vulkan Quake3 Engine§1:Prelude*/
#define VK_USE_PLATFORM_XLIB_KHR
#include<vulkan/vulkan.h>
#include<SDL2/SDL.h>
#include<SDL2/SDL_vulkan.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>
#include<stdint.h>
#include<stdbool.h>
#define S static
#define U uint32_t
#define U8 uint8_t
#define I int32_t
#define I16 int16_t
#define F float
#define V void
#define C const
#define R return
#define L(n,b) for(U i=0;i<n;i++){b}
#define L2(n,v,b) for(U v=0;v<n;v++){b}
#define M(s) malloc(s)
#define Z(p,s) memset(p,0,s)
#define CP(d,s,n) memcpy(d,s,n)
#define W(f,...) fprintf(stderr,f,__VA_ARGS__)
#define E(c,m) if(c){W("%s\n",m);exit(1);}
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define CL(x,a,b) MIN(MAX(x,a),b)
#define V3(x,y,z) {x,y,z}
#define V4(x,y,z,w) {x,y,z,w}
#define PI 3.14159265359
typedef struct{F x,y,z;}v3;
typedef struct{F x,y,z,w;}v4;
typedef struct{I x,y,z,w;}i4;
typedef struct{F m[16];}m4;
/*§2:Vector Math*/
S v3 v3add(v3 a,v3 b){R(v3){a.x+b.x,a.y+b.y,a.z+b.z};}
S v3 v3sub(v3 a,v3 b){R(v3){a.x-b.x,a.y-b.y,a.z-b.z};}
S v3 v3mul(v3 a,F s){R(v3){a.x*s,a.y*s,a.z*s};}
S F v3dot(v3 a,v3 b){R a.x*b.x+a.y*b.y+a.z*b.z;}
S F v3len(v3 v){R sqrtf(v3dot(v,v));}
S v3 v3norm(v3 v){F l=v3len(v);R l>0?v3mul(v,1/l):v;}
S v3 v3cross(v3 a,v3 b){R(v3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
S m4 m4ident(){m4 m;Z(&m,sizeof(m));m.m[0]=m.m[5]=m.m[10]=m.m[15]=1;R m;}
S m4 m4persp(F fov,F asp,F n,F f){m4 m;Z(&m,sizeof(m));F t=tanf(fov/2),r=t*asp;m.m[0]=1/r;m.m[5]=1/t;m.m[10]=f/(f-n);m.m[11]=1;m.m[14]=-f*n/(f-n);R m;}
S m4 m4look(v3 e,v3 c,v3 u){v3 f=v3norm(v3sub(c,e)),r=v3norm(v3cross(f,u)),up=v3cross(r,f);m4 m;m.m[0]=r.x;m.m[4]=r.y;m.m[8]=r.z;m.m[12]=-v3dot(r,e);m.m[1]=up.x;m.m[5]=up.y;m.m[9]=up.z;m.m[13]=-v3dot(up,e);m.m[2]=-f.x;m.m[6]=-f.y;m.m[10]=-f.z;m.m[14]=v3dot(f,e);m.m[3]=0;m.m[7]=0;m.m[11]=0;m.m[15]=1;R m;}
S m4 m4mul(m4 a,m4 b){m4 r;L(16,r.m[i]=0;)for(U i=0;i<4;i++)for(U j=0;j<4;j++)for(U k=0;k<4;k++)r.m[i*4+j]+=a.m[i*4+k]*b.m[k*4+j];R r;}
/*§3:BSP Structures*/
#define BSP_MAGIC 0x50534249
#define BSP_VER 0x2e
typedef struct{I magic,ver;}BH;
typedef struct{I off,len;}BD;
typedef struct{v3 min,max;}BB;
typedef struct{v3 p;v3 n;F d;}BP;
typedef struct{char name[64];I flags,contents;}BT;
typedef struct{v3 p;F uv[2][2];v3 n;U8 c[4];}BV;
typedef struct{I tex,eff,type,v,nv,mi,nm,lm,lc[2],ls[2],ln,cp[3];}BF;
typedef struct{char n[64];U8*d;I w,h;}TX;
typedef struct{U8*d;I w,h;}LM;
typedef struct{I pl,ch[2];}BN;
typedef struct{I c,b;}BL;
typedef struct{I l,nb,bb,nbr,br;}BM;
typedef struct{I a,na,f,nf,fb,nfb,p,np,n,nn,l,nl,lf,nlf;}BVD;
/*§4:MD3 Structures*/
#define MD3_MAGIC 0x33504449
#define MD3_VER 15
typedef struct{I magic,ver,name[16],flags,nfr,ntg,nme,nsk,off_fr,off_tg,off_sf,off_end;}M3H;
typedef struct{v3 min,max,org;F r;char n[16];}M3F;
typedef struct{char n[64];I sh;}M3T;
typedef struct{char n[64];I off_tri,off_tc,off_v,off_end,nv,nt;}M3S;
typedef struct{I idx[3];}M3I;
typedef struct{F uv[2];}M3C;
typedef struct{I16 p[3];U8 n[2];}M3V;
typedef struct{M3S*s;M3I*tri;M3C*tc;M3V*v;U nv,nt;}M3D;
/*§5:Global State*/
typedef struct{
SDL_Window*w;VkInstance i;VkSurfaceKHR s;VkPhysicalDevice pd;VkDevice d;VkQueue q;U qi;VkSwapchainKHR sc;VkImage*si;VkImageView*sv;U sic;VkRenderPass rp;VkPipelineLayout pl;VkPipeline p;VkCommandPool cp;VkCommandBuffer*cb;VkSemaphore ia,rf;VkFence*ff;VkBuffer vb,ib,ub;VkDeviceMemory vm,im,um;VkDescriptorSetLayout dsl;VkDescriptorPool dp;VkDescriptorSet*ds;VkImage ti,di;VkImageView tiv,div;VkDeviceMemory tm,dm;VkSampler smp;U ww,hh;
}VK;
typedef struct{
BV*v;BF*f;BT*tx;BP*pl;BN*n;I*lf;BL*lv;BM*m;BVD vd;TX*tex;LM*lm;U nv,nf,nt,np,nn,nlf,nlv,nm,ntex,nlm;U8*vis;v3 sp;
}MAP;
typedef struct{
M3D*md;TX*tx;U nm,nt;
}MDL;
typedef struct{
v3 p,a;F ya,pi,sp;I mv,st;
}PL;
S VK vk;S MAP mp;S MDL mdl[64];S U nmdl;S PL pl;S U ft,cfi;
/*§6:File IO*/
S U8*rd(C char*p,U*sz){FILE*f=fopen(p,"rb");if(!f){W("Failed to open: %s\n",p);R 0;}fseek(f,0,SEEK_END);*sz=ftell(f);fseek(f,0,SEEK_SET);U8*d=M(*sz);fread(d,1,*sz,f);fclose(f);R d;}
/*§7:TGA Loader*/
S TX rdtga(C char*p){TX t;Z(&t,sizeof(t));U sz;U8*d=rd(p,&sz);if(!d||sz<18){if(d)free(d);R t;}U8 idt=d[0],cmt=d[1],it=d[2];I w=d[12]|(d[13]<<8),h=d[14]|(d[15]<<8),bpp=d[16];if((it!=2&&it!=10)||(bpp!=24&&bpp!=32)){free(d);R t;}U8*img=d+18+idt;t.w=w;t.h=h;t.d=M(w*h*4);I pc=bpp/8;if(it==2){L(w*h,U p=i*pc;t.d[i*4]=img[p+2];t.d[i*4+1]=img[p+1];t.d[i*4+2]=img[p];t.d[i*4+3]=pc==4?img[p+3]:255;)}else{U p=0,op=0;while(op<w*h){U8 hh=img[p++];I c=1+(hh&0x7f);I rl=hh&0x80;if(rl){U8 px[4]={img[p+2],img[p+1],img[p],pc==4?img[p+3]:255};p+=pc;L(c,if(op>=w*h)break;CP(&t.d[op*4],px,4);op++;)}else{L(c,if(op>=w*h)break;t.d[op*4]=img[p+2];t.d[op*4+1]=img[p+1];t.d[op*4+2]=img[p];t.d[op*4+3]=pc==4?img[p+3]:255;p+=pc;op++;)}}}free(d);R t;}
/*§8:BSP Loader*/
S V rdbsp(C char*p){U sz;U8*d=rd(p,&sz);if(!d)exit(1);BH*h=(BH*)d;E(h->magic!=BSP_MAGIC||h->ver!=BSP_VER,"Invalid BSP");BD*de=(BD*)(d+8);
#define DE(i) (&de[i])
#define DD(i) (d+DE(i)->off)
#define DN(i,t) (DE(i)->len/sizeof(t))
BT*tx=(BT*)DD(1);U ntx=DN(1,BT);BP*pl=(BP*)DD(2);U npl=DN(2,BP);BN*n=(BN*)DD(3);U nn=DN(3,BN);I*lf=(I*)DD(4);U nlf=DN(4,I);BV*v=(BV*)DD(10);U nv=DN(10,BV);I*mi=(I*)DD(11);BF*f=(BF*)DD(13);U nf=DN(13,BF);BL*lv=(BL*)DD(16);U nlv=DN(16,BL);BM*m=(BM*)DD(17);U nm=DN(17,BM);BVD*vd=(BVD*)DD(18);U8*vis=DD(16)+(vd->a*vd->na);mp.v=M(nv*sizeof(BV));CP(mp.v,v,nv*sizeof(BV));mp.nv=nv;mp.f=M(nf*sizeof(BF));CP(mp.f,f,nf*sizeof(BF));mp.nf=nf;mp.tx=M(ntx*sizeof(BT));CP(mp.tx,tx,ntx*sizeof(BT));mp.nt=ntx;mp.pl=M(npl*sizeof(BP));CP(mp.pl,pl,npl*sizeof(BP));mp.np=npl;mp.n=M(nn*sizeof(BN));CP(mp.n,n,nn*sizeof(BN));mp.nn=nn;mp.lf=M(nlf*sizeof(I));CP(mp.lf,lf,nlf*sizeof(I));mp.nlf=nlf;mp.lv=M(nlv*sizeof(BL));CP(mp.lv,lv,nlv*sizeof(BL));mp.nlv=nlv;mp.m=M(nm*sizeof(BM));CP(mp.m,m,nm*sizeof(BM));mp.nm=nm;CP(&mp.vd,vd,sizeof(BVD));mp.vis=M(vd->a*vd->na);CP(mp.vis,vis,vd->a*vd->na);mp.tex=M(ntx*sizeof(TX));mp.ntex=ntx;mp.lm=M(nf*sizeof(LM));mp.nlm=nf;char pb[512];L(ntx,char*nm=mp.tx[i].name;I j=0;while(nm[j]){if(nm[j]=='/'||nm[j]=='\\')nm=&nm[j+1];j++;}sprintf(pb,"assets/textures/%s.tga",nm);FILE*tf=fopen(pb,"rb");if(tf){fclose(tf);mp.tex[i]=rdtga(pb);}else{mp.tex[i].w=64;mp.tex[i].h=64;mp.tex[i].d=M(64*64*4);L(64*64,mp.tex[i].d[j*4]=(j/64+j%64)%2?255:0;mp.tex[i].d[j*4+1]=0;mp.tex[i].d[j*4+2]=(j/64+j%64)%2?0:255;mp.tex[i].d[j*4+3]=255;)})L(nf,U8*ld=(U8*)DD(14)+f[i].lc[0]*3*128*128;mp.lm[i].w=128;mp.lm[i].h=128;mp.lm[i].d=M(128*128*4);L2(128*128,j,mp.lm[i].d[j*4]=ld[j*3];mp.lm[i].d[j*4+1]=ld[j*3+1];mp.lm[i].d[j*4+2]=ld[j*3+2];mp.lm[i].d[j*4+3]=255;))v3 mn=v[0].p,mx=v[0].p;L(nv,mn.x=MIN(mn.x,v[i].p.x);mn.y=MIN(mn.y,v[i].p.y);mn.z=MIN(mn.z,v[i].p.z);mx.x=MAX(mx.x,v[i].p.x);mx.y=MAX(mx.y,v[i].p.y);mx.z=MAX(mx.z,v[i].p.z);)mp.sp=(v3){(mn.x+mx.x)/2,(mn.y+mx.y)/2,(mn.z+mx.z)/2+100};free(d);}
/*§9:MD3 Loader*/
S M3D rdmd3(C char*p){M3D md;Z(&md,sizeof(md));U sz;U8*d=rd(p,&sz);if(!d)R md;M3H*h=(M3H*)d;if(h->magic!=MD3_MAGIC||h->ver!=MD3_VER){free(d);R md;}md.s=M(h->nme*sizeof(M3S));M3S*ss=(M3S*)(d+h->off_sf);L(h->nme,CP(&md.s[i],&ss[i],sizeof(M3S));)free(d);R md;}
/*§10:Vulkan Init*/
S U fndmem(U tf,U pf){VkPhysicalDeviceMemoryProperties mp;vkGetPhysicalDeviceMemoryProperties(vk.pd,&mp);L(mp.memoryTypeCount,if((tf&(1<<i))&&(mp.memoryTypes[i].propertyFlags&pf)==pf)R i;)R-1;}
S V crbuf(VkDeviceSize sz,VkBufferUsageFlags u,VkMemoryPropertyFlags p,VkBuffer*b,VkDeviceMemory*m){VkBufferCreateInfo bi={VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,0,0,sz,u,VK_SHARING_MODE_EXCLUSIVE,0,0};vkCreateBuffer(vk.d,&bi,0,b);VkMemoryRequirements mr;vkGetBufferMemoryRequirements(vk.d,*b,&mr);VkMemoryAllocateInfo ai={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,0,mr.size,fndmem(mr.memoryTypeBits,p)};vkAllocateMemory(vk.d,&ai,0,m);vkBindBufferMemory(vk.d,*b,*m,0);}
S V crimg(U w,U h,VkFormat f,VkImageTiling t,VkImageUsageFlags u,VkMemoryPropertyFlags p,VkImage*i,VkDeviceMemory*m){VkImageCreateInfo ci={VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,0,0,VK_IMAGE_TYPE_2D,f,{w,h,1},1,1,VK_SAMPLE_COUNT_1_BIT,t,u,VK_SHARING_MODE_EXCLUSIVE,0,0,VK_IMAGE_LAYOUT_UNDEFINED};vkCreateImage(vk.d,&ci,0,i);VkMemoryRequirements mr;vkGetImageMemoryRequirements(vk.d,*i,&mr);VkMemoryAllocateInfo ai={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,0,mr.size,fndmem(mr.memoryTypeBits,p)};vkAllocateMemory(vk.d,&ai,0,m);vkBindImageMemory(vk.d,*i,*m,0);}
S V cpbuf(VkBuffer s,VkBuffer d,VkDeviceSize sz){VkCommandBufferAllocateInfo ai={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,0,vk.cp,VK_COMMAND_BUFFER_LEVEL_PRIMARY,1};VkCommandBuffer cb;vkAllocateCommandBuffers(vk.d,&ai,&cb);VkCommandBufferBeginInfo bi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,0,VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,0};vkBeginCommandBuffer(cb,&bi);VkBufferCopy r={0,0,sz};vkCmdCopyBuffer(cb,s,d,1,&r);vkEndCommandBuffer(cb);VkSubmitInfo si={VK_STRUCTURE_TYPE_SUBMIT_INFO,0,0,0,0,1,&cb,0,0};vkQueueSubmit(vk.q,1,&si,0);vkQueueWaitIdle(vk.q);vkFreeCommandBuffers(vk.d,vk.cp,1,&cb);}
S V trimg(VkImage i,VkFormat f,VkImageLayout ol,VkImageLayout nl){VkCommandBufferAllocateInfo ai={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,0,vk.cp,VK_COMMAND_BUFFER_LEVEL_PRIMARY,1};VkCommandBuffer cb;vkAllocateCommandBuffers(vk.d,&ai,&cb);VkCommandBufferBeginInfo bi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,0,VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,0};vkBeginCommandBuffer(cb,&bi);VkImageMemoryBarrier b={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,0,0,0,ol,nl,VK_QUEUE_FAMILY_IGNORED,VK_QUEUE_FAMILY_IGNORED,i,{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};VkPipelineStageFlags ss,ds;if(ol==VK_IMAGE_LAYOUT_UNDEFINED&&nl==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL){b.srcAccessMask=0;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;ss=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;ds=VK_PIPELINE_STAGE_TRANSFER_BIT;}else{b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;ss=VK_PIPELINE_STAGE_TRANSFER_BIT;ds=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;}vkCmdPipelineBarrier(cb,ss,ds,0,0,0,0,0,1,&b);vkEndCommandBuffer(cb);VkSubmitInfo si={VK_STRUCTURE_TYPE_SUBMIT_INFO,0,0,0,0,1,&cb,0,0};vkQueueSubmit(vk.q,1,&si,0);vkQueueWaitIdle(vk.q);vkFreeCommandBuffers(vk.d,vk.cp,1,&cb);}
S V cpbuftoimg(VkBuffer b,VkImage i,U w,U h){VkCommandBufferAllocateInfo ai={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,0,vk.cp,VK_COMMAND_BUFFER_LEVEL_PRIMARY,1};VkCommandBuffer cb;vkAllocateCommandBuffers(vk.d,&ai,&cb);VkCommandBufferBeginInfo bi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,0,VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,0};vkBeginCommandBuffer(cb,&bi);VkBufferImageCopy r={0,0,0,{VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},{0,0,0},{w,h,1}};vkCmdCopyBufferToImage(cb,b,i,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&r);vkEndCommandBuffer(cb);VkSubmitInfo si={VK_STRUCTURE_TYPE_SUBMIT_INFO,0,0,0,0,1,&cb,0,0};vkQueueSubmit(vk.q,1,&si,0);vkQueueWaitIdle(vk.q);vkFreeCommandBuffers(vk.d,vk.cp,1,&cb);}
S V upbuf(VkBuffer b,VkDeviceMemory m,V*data,VkDeviceSize sz){V*d;vkMapMemory(vk.d,m,0,sz,0,&d);CP(d,data,sz);vkUnmapMemory(vk.d,m);}
S V crtex(){U tw=256,th=256;VkDeviceSize sz=tw*th*4;VkBuffer sb;VkDeviceMemory sm;crbuf(sz,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&sb,&sm);U8*td=M(sz);L(tw*th,td[i*4]=(i%tw+i/tw)%2?255:128;td[i*4+1]=128;td[i*4+2]=(i%tw+i/tw)%2?128:255;td[i*4+3]=255;)upbuf(sb,sm,td,sz);free(td);crimg(tw,th,VK_FORMAT_R8G8B8A8_SRGB,VK_IMAGE_TILING_OPTIMAL,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&vk.ti,&vk.tm);trimg(vk.ti,VK_FORMAT_R8G8B8A8_SRGB,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);cpbuftoimg(sb,vk.ti,tw,th);trimg(vk.ti,VK_FORMAT_R8G8B8A8_SRGB,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);vkDestroyBuffer(vk.d,sb,0);vkFreeMemory(vk.d,sm,0);VkImageViewCreateInfo ivci={VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,0,0,vk.ti,VK_IMAGE_VIEW_TYPE_2D,VK_FORMAT_R8G8B8A8_SRGB,{},{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};vkCreateImageView(vk.d,&ivci,0,&vk.tiv);crimg(vk.ww,vk.hh,VK_FORMAT_D32_SFLOAT,VK_IMAGE_TILING_OPTIMAL,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&vk.di,&vk.dm);VkImageViewCreateInfo dvci={VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,0,0,vk.di,VK_IMAGE_VIEW_TYPE_2D,VK_FORMAT_D32_SFLOAT,{},{VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1}};vkCreateImageView(vk.d,&dvci,0,&vk.div);VkSamplerCreateInfo sci={VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,0,0,VK_FILTER_LINEAR,VK_FILTER_LINEAR,VK_SAMPLER_MIPMAP_MODE_LINEAR,VK_SAMPLER_ADDRESS_MODE_REPEAT,VK_SAMPLER_ADDRESS_MODE_REPEAT,VK_SAMPLER_ADDRESS_MODE_REPEAT,0,0,1,0,VK_COMPARE_OP_ALWAYS,0,0,VK_BORDER_COLOR_INT_OPAQUE_BLACK,0};vkCreateSampler(vk.d,&sci,0,&vk.smp);}
S V crvkbuf(){VkDeviceSize vsz=mp.nv*sizeof(BV),isz=mp.nf*6*sizeof(U);U*idx=M(isz);U ic=0;L(mp.nf,BF*f=&mp.f[i];if(f->type==1||f->type==3){U b=f->v;L2(f->nv-2,j,idx[ic++]=b;idx[ic++]=b+j+1;idx[ic++]=b+j+2;)})VkBuffer sb,ib;VkDeviceMemory smi,imi;crbuf(vsz,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&sb,&smi);crbuf(isz,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&ib,&imi);upbuf(sb,smi,mp.v,vsz);upbuf(ib,imi,idx,isz);crbuf(vsz,VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&vk.vb,&vk.vm);crbuf(isz,VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&vk.ib,&vk.im);cpbuf(sb,vk.vb,vsz);cpbuf(ib,vk.ib,isz);vkDestroyBuffer(vk.d,sb,0);vkFreeMemory(vk.d,smi,0);vkDestroyBuffer(vk.d,ib,0);vkFreeMemory(vk.d,imi,0);free(idx);crbuf(sizeof(m4)*2,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&vk.ub,&vk.um);}
S VkShaderModule crsh(C char*path){U sz;U8*code=rd(path,&sz);E(!code,"Failed to read shader");VkShaderModuleCreateInfo ci={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,0,0,sz,(U*)code};VkShaderModule sm;vkCreateShaderModule(vk.d,&ci,0,&sm);free(code);R sm;}
S V initvk(){SDL_Init(SDL_INIT_VIDEO);vk.ww=1920;vk.hh=1080;vk.w=SDL_CreateWindow("Q3VK",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,vk.ww,vk.hh,SDL_WINDOW_VULKAN|SDL_WINDOW_SHOWN);U ec;SDL_Vulkan_GetInstanceExtensions(vk.w,&ec,0);C char**en=M(ec*sizeof(char*));SDL_Vulkan_GetInstanceExtensions(vk.w,&ec,en);VkApplicationInfo ai={VK_STRUCTURE_TYPE_APPLICATION_INFO,0,"Q3VK",1,"Q3VK",1,VK_API_VERSION_1_0};VkInstanceCreateInfo ici={VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,0,0,&ai,0,0,ec,en};vkCreateInstance(&ici,0,&vk.i);SDL_Vulkan_CreateSurface(vk.w,vk.i,&vk.s);U pdc=0;vkEnumeratePhysicalDevices(vk.i,&pdc,0);VkPhysicalDevice*pds=M(pdc*sizeof(VkPhysicalDevice));vkEnumeratePhysicalDevices(vk.i,&pdc,pds);vk.pd=pds[0];free(pds);U qfc=0;vkGetPhysicalDeviceQueueFamilyProperties(vk.pd,&qfc,0);VkQueueFamilyProperties*qfp=M(qfc*sizeof(VkQueueFamilyProperties));vkGetPhysicalDeviceQueueFamilyProperties(vk.pd,&qfc,qfp);vk.qi=-1;L(qfc,VkBool32 ps;vkGetPhysicalDeviceSurfaceSupportKHR(vk.pd,i,vk.s,&ps);if(qfp[i].queueFlags&VK_QUEUE_GRAPHICS_BIT&&ps){vk.qi=i;break;})free(qfp);E(vk.qi==-1,"No queue");F qp=1;VkDeviceQueueCreateInfo qci={VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,0,0,vk.qi,1,&qp};C char*de[]={VK_KHR_SWAPCHAIN_EXTENSION_NAME};VkPhysicalDeviceFeatures pdf={};VkDeviceCreateInfo dci={VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,0,0,1,&qci,0,0,1,de,&pdf};vkCreateDevice(vk.pd,&dci,0,&vk.d);vkGetDeviceQueue(vk.d,vk.qi,0,&vk.q);VkSurfaceCapabilitiesKHR cap;vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.pd,vk.s,&cap);vk.sic=cap.minImageCount+1;if(cap.maxImageCount>0&&vk.sic>cap.maxImageCount)vk.sic=cap.maxImageCount;VkSwapchainCreateInfoKHR sci={VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,0,0,vk.s,vk.sic,VK_FORMAT_B8G8R8A8_SRGB,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,{vk.ww,vk.hh},1,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,VK_SHARING_MODE_EXCLUSIVE,0,0,cap.currentTransform,VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,VK_PRESENT_MODE_FIFO_KHR,1,0};vkCreateSwapchainKHR(vk.d,&sci,0,&vk.sc);vkGetSwapchainImagesKHR(vk.d,vk.sc,&vk.sic,0);vk.si=M(vk.sic*sizeof(VkImage));vkGetSwapchainImagesKHR(vk.d,vk.sc,&vk.sic,vk.si);vk.sv=M(vk.sic*sizeof(VkImageView));L(vk.sic,VkImageViewCreateInfo ivci={VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,0,0,vk.si[i],VK_IMAGE_VIEW_TYPE_2D,VK_FORMAT_B8G8R8A8_SRGB,{},{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};vkCreateImageView(vk.d,&ivci,0,&vk.sv[i]);)crtex();VkAttachmentDescription ad[2]={{0,VK_FORMAT_B8G8R8A8_SRGB,VK_SAMPLE_COUNT_1_BIT,VK_ATTACHMENT_LOAD_OP_CLEAR,VK_ATTACHMENT_STORE_OP_STORE,VK_ATTACHMENT_LOAD_OP_DONT_CARE,VK_ATTACHMENT_STORE_OP_DONT_CARE,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},{0,VK_FORMAT_D32_SFLOAT,VK_SAMPLE_COUNT_1_BIT,VK_ATTACHMENT_LOAD_OP_CLEAR,VK_ATTACHMENT_STORE_OP_DONT_CARE,VK_ATTACHMENT_LOAD_OP_DONT_CARE,VK_ATTACHMENT_STORE_OP_DONT_CARE,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}};VkAttachmentReference car={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkAttachmentReference dar={1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};VkSubpassDescription sd={0,VK_PIPELINE_BIND_POINT_GRAPHICS,0,0,1,&car,0,&dar,0,0};VkSubpassDependency dep={VK_SUBPASS_EXTERNAL,0,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,0,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,0};VkRenderPassCreateInfo rpci={VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,0,0,2,ad,1,&sd,1,&dep};vkCreateRenderPass(vk.d,&rpci,0,&vk.rp);VkDescriptorSetLayoutBinding bs[3]={{0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT,0},{1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,0},{2,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,0}};VkDescriptorSetLayoutCreateInfo dslci={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,0,0,3,bs};vkCreateDescriptorSetLayout(vk.d,&dslci,0,&vk.dsl);VkPushConstantRange pcr={VK_SHADER_STAGE_FRAGMENT_BIT,0,4};VkPipelineLayoutCreateInfo plci={VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,0,0,1,&vk.dsl,1,&pcr};vkCreatePipelineLayout(vk.d,&plci,0,&vk.pl);VkShaderModule vsm=crsh("q3.vert.glsl.spv"),fsm=crsh("q3.frag.glsl.spv");VkPipelineShaderStageCreateInfo ssc[2]={{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,0,0,VK_SHADER_STAGE_VERTEX_BIT,vsm,"main",0},{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,0,0,VK_SHADER_STAGE_FRAGMENT_BIT,fsm,"main",0}};VkVertexInputBindingDescription vbd={0,sizeof(BV),VK_VERTEX_INPUT_RATE_VERTEX};VkVertexInputAttributeDescription vad[5]={{0,0,VK_FORMAT_R32G32B32_SFLOAT,0},{1,0,VK_FORMAT_R32G32_SFLOAT,12},{2,0,VK_FORMAT_R32G32_SFLOAT,20},{3,0,VK_FORMAT_R32G32B32_SFLOAT,28},{4,0,VK_FORMAT_R8G8B8A8_UNORM,40}};VkPipelineVertexInputStateCreateInfo visc={VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,0,0,1,&vbd,5,vad};VkPipelineInputAssemblyStateCreateInfo iasc={VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,0,0,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,0};VkViewport vp={0,0,vk.ww,vk.hh,0,1};VkRect2D sc={{0,0},{vk.ww,vk.hh}};VkPipelineViewportStateCreateInfo vpsc={VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,0,0,1,&vp,1,&sc};VkPipelineRasterizationStateCreateInfo rsc={VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,0,0,0,0,VK_POLYGON_MODE_FILL,VK_CULL_MODE_BACK_BIT,VK_FRONT_FACE_CLOCKWISE,0,0,0,0,1};VkPipelineMultisampleStateCreateInfo msc={VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,0,0,VK_SAMPLE_COUNT_1_BIT,0,0,0,0,0};VkPipelineColorBlendAttachmentState cba={0,VK_BLEND_FACTOR_SRC_ALPHA,VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,VK_BLEND_OP_ADD,VK_BLEND_FACTOR_ONE,VK_BLEND_FACTOR_ZERO,VK_BLEND_OP_ADD,0xf};VkPipelineColorBlendStateCreateInfo cbsc={VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,0,0,0,0,1,&cba,{0,0,0,0}};VkPipelineDepthStencilStateCreateInfo dssc={VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,0,0,1,1,VK_COMPARE_OP_LESS,0,0,{},{},0,1};VkGraphicsPipelineCreateInfo gpci={VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,0,0,2,ssc,&visc,&iasc,0,&vpsc,&rsc,&msc,&dssc,&cbsc,0,vk.pl,vk.rp,0,0,0};vkCreateGraphicsPipelines(vk.d,0,1,&gpci,0,&vk.p);vkDestroyShaderModule(vk.d,vsm,0);vkDestroyShaderModule(vk.d,fsm,0);VkCommandPoolCreateInfo cpci={VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,0,VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,vk.qi};vkCreateCommandPool(vk.d,&cpci,0,&vk.cp);vk.cb=M(vk.sic*sizeof(VkCommandBuffer));VkCommandBufferAllocateInfo cbai={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,0,vk.cp,VK_COMMAND_BUFFER_LEVEL_PRIMARY,vk.sic};vkAllocateCommandBuffers(vk.d,&cbai,vk.cb);VkSemaphoreCreateInfo smci={VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,0,0};vkCreateSemaphore(vk.d,&smci,0,&vk.ia);vkCreateSemaphore(vk.d,&smci,0,&vk.rf);vk.ff=M(vk.sic*sizeof(VkFence));VkFenceCreateInfo fci={VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,0,VK_FENCE_CREATE_SIGNALED_BIT};L(vk.sic,vkCreateFence(vk.d,&fci,0,&vk.ff[i]);)VkDescriptorPoolSize ps[2]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,vk.sic},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,vk.sic*2}};VkDescriptorPoolCreateInfo dpci={VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,0,0,vk.sic,2,ps};vkCreateDescriptorPool(vk.d,&dpci,0,&vk.dp);vk.ds=M(vk.sic*sizeof(VkDescriptorSet));VkDescriptorSetLayout*ls=M(vk.sic*sizeof(VkDescriptorSetLayout));L(vk.sic,ls[i]=vk.dsl;)VkDescriptorSetAllocateInfo dsai={VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,0,vk.dp,vk.sic,ls};vkAllocateDescriptorSets(vk.d,&dsai,vk.ds);for(U i=0;i<vk.sic;i++){VkDescriptorBufferInfo bi={vk.ub,0,sizeof(m4)*2};VkDescriptorImageInfo ii={vk.smp,vk.tiv,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};VkDescriptorImageInfo li={vk.smp,vk.tiv,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};VkWriteDescriptorSet wd[3]={{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,0,vk.ds[i],0,0,1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,0,&bi,0},{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,0,vk.ds[i],1,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&ii,0,0},{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,0,vk.ds[i],2,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&li,0,0}};vkUpdateDescriptorSets(vk.d,3,wd,0,0);}free(ls);}
/*§11:Rendering*/
S V drw(){vkWaitForFences(vk.d,1,&vk.ff[cfi],1,-1);vkResetFences(vk.d,1,&vk.ff[cfi]);U ii;vkAcquireNextImageKHR(vk.d,vk.sc,-1,vk.ia,0,&ii);m4 v=m4look(pl.p,(v3){pl.p.x+cosf(pl.ya)*cosf(pl.pi),pl.p.y+sinf(pl.ya)*cosf(pl.pi),pl.p.z+sinf(pl.pi)},(v3){0,0,1});m4 p=m4persp(1.22,vk.ww/(F)vk.hh,.1,4096);m4 ub[2]={v,p};upbuf(vk.ub,vk.um,ub,sizeof(ub));vkResetCommandBuffer(vk.cb[ii],0);VkCommandBufferBeginInfo bi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,0,0,0};vkBeginCommandBuffer(vk.cb[ii],&bi);VkRenderPassBeginInfo rpbi={VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,0,vk.rp,0,{{0,0},{vk.ww,vk.hh}},2,(VkClearValue[]){{.color={{.1,.1,.15,1}}},{.depthStencil={1,0}}}};VkFramebuffer fb;VkImageView av[2]={vk.sv[ii],vk.div};VkFramebufferCreateInfo fci={VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,0,0,vk.rp,2,av,vk.ww,vk.hh,1};vkCreateFramebuffer(vk.d,&fci,0,&fb);rpbi.framebuffer=fb;vkCmdBeginRenderPass(vk.cb[ii],&rpbi,VK_SUBPASS_CONTENTS_INLINE);vkCmdBindPipeline(vk.cb[ii],VK_PIPELINE_BIND_POINT_GRAPHICS,vk.p);VkDeviceSize of=0;vkCmdBindVertexBuffers(vk.cb[ii],0,1,&vk.vb,&of);vkCmdBindIndexBuffer(vk.cb[ii],vk.ib,0,VK_INDEX_TYPE_UINT32);vkCmdBindDescriptorSets(vk.cb[ii],VK_PIPELINE_BIND_POINT_GRAPHICS,vk.pl,0,1,&vk.ds[ii],0,0);U ic=0;L(mp.nf,BF*f=&mp.f[i];if(f->type==1||f->type==3){U c=(f->nv-2)*3;U m=0;vkCmdPushConstants(vk.cb[ii],vk.pl,VK_SHADER_STAGE_FRAGMENT_BIT,0,4,&m);vkCmdDrawIndexed(vk.cb[ii],c,1,ic,0,0);ic+=c;})vkCmdEndRenderPass(vk.cb[ii]);vkEndCommandBuffer(vk.cb[ii]);VkPipelineStageFlags wm=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;VkSubmitInfo si={VK_STRUCTURE_TYPE_SUBMIT_INFO,0,1,&vk.ia,&wm,1,&vk.cb[ii],1,&vk.rf};vkQueueSubmit(vk.q,1,&si,vk.ff[cfi]);VkPresentInfoKHR pi={VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,0,1,&vk.rf,1,&vk.sc,&ii,0};vkQueuePresentKHR(vk.q,&pi);vkDestroyFramebuffer(vk.d,fb,0);cfi=(cfi+1)%vk.sic;}
/*§12:Input & Game Loop*/
S V inp(){SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_QUIT)exit(0);if(e.type==SDL_KEYDOWN){I k=e.key.keysym.sym;if(k==SDLK_w)pl.mv|=1;if(k==SDLK_s)pl.mv|=2;if(k==SDLK_a)pl.mv|=4;if(k==SDLK_d)pl.mv|=8;if(k==SDLK_SPACE)pl.mv|=16;if(k==SDLK_LSHIFT)pl.mv|=32;if(k==SDLK_ESCAPE)exit(0);}if(e.type==SDL_KEYUP){I k=e.key.keysym.sym;if(k==SDLK_w)pl.mv&=~1;if(k==SDLK_s)pl.mv&=~2;if(k==SDLK_a)pl.mv&=~4;if(k==SDLK_d)pl.mv&=~8;if(k==SDLK_SPACE)pl.mv&=~16;if(k==SDLK_LSHIFT)pl.mv&=~32;}if(e.type==SDL_MOUSEMOTION){pl.ya+=e.motion.xrel*.002;pl.pi-=e.motion.yrel*.002;pl.pi=CL(pl.pi,-1.57,1.57);}}SDL_SetRelativeMouseMode(1);}
S V upd(F dt){v3 fw={cosf(pl.ya),sinf(pl.ya),0};v3 rt={-sinf(pl.ya),cosf(pl.ya),0};v3 mv={0,0,0};F sp=300*dt;if(pl.mv&1)mv=v3add(mv,v3mul(fw,sp));if(pl.mv&2)mv=v3add(mv,v3mul(fw,-sp));if(pl.mv&4)mv=v3add(mv,v3mul(rt,-sp));if(pl.mv&8)mv=v3add(mv,v3mul(rt,sp));if(pl.mv&16)mv.z+=sp;if(pl.mv&32)mv.z-=sp;pl.p=v3add(pl.p,mv);}
/*§13:Main*/
I main(I ac,char**av){
char*mp_path=ac>1?av[1]:"assets/maps/oa_dm4.bsp";
printf("Loading BSP: %s\n",mp_path);
rdbsp(mp_path);
printf("BSP loaded: %u vertices, %u faces\n",mp.nv,mp.nf);
initvk();
crvkbuf();
pl.p=mp.sp;
pl.ya=pl.pi=0;
pl.sp=300;
pl.mv=0;
U lt=SDL_GetTicks();
while(1){
U ct=SDL_GetTicks();
F dt=(ct-lt)/1000.0;
if(dt>0.1)dt=0.1;
lt=ct;
inp();
upd(dt);
drw();
SDL_Delay(1);
}
R 0;
}
