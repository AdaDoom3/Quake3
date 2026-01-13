#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>
#include<math.h>
#include<assert.h>

typedef union{struct{float x,y,z;};float v[3];}v3;
typedef union{struct{float x,y,z,w;};v3 xyz;float v[4];}v4;
typedef union{struct{int x,y;};int v[2];}i2;
typedef union{struct{int x,y,z;};int v[3];}i3;
typedef union{struct{uint8_t r,g,b,a;};uint32_t u;}rgba;
typedef float m4[16];

#define V3(X,Y,Z) ((v3){{X,Y,Z}})
#define V4(X,Y,Z,W) ((v4){{X,Y,Z,W}})

static inline v3 v3add(v3 a,v3 b){return V3(a.x+b.x,a.y+b.y,a.z+b.z);}
static inline v3 v3sub(v3 a,v3 b){return V3(a.x-b.x,a.y-b.y,a.z-b.z);}
static inline v3 v3mul(v3 a,float s){return V3(a.x*s,a.y*s,a.z*s);}
static inline float v3dot(v3 a,v3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static inline v3 v3cross(v3 a,v3 b){return V3(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
static inline float v3len(v3 a){return sqrtf(v3dot(a,a));}
static inline v3 v3norm(v3 a){float l=v3len(a);return l>1e-6f?v3mul(a,1.f/l):V3(0,0,0);}

typedef struct{int o,l;}Lump;
typedef struct{char sig[4];int ver;Lump lm[17];}BSPHdr;
typedef struct{v3 p;float uv[2][2];v3 n;rgba c;}BSPVert;
typedef struct{v3 n;float d;}BSPPlane;
typedef struct{int pl;int ch[2];}BSPNode;
typedef struct{int cl,ar;i3 mn,mx;int lf,nf,lb,nb;}BSPLeaf;
typedef struct{char nm[64];int fl,ct;}BSPShader;
typedef struct{i3 mn,mx;int fc,n,br,nb;}BSPModel;

typedef struct{
    BSPVert*verts;uint32_t nv;
    uint32_t*idx;uint32_t ni;
    BSPPlane*pl;uint32_t np;
    BSPNode*nd;uint32_t nn;
    BSPLeaf*lf;uint32_t nl;
    int*lffc;uint32_t nlf;
    BSPShader*sh;uint32_t ns;
    uint8_t*lm;uint32_t nlm;
    char*ent;
}BSP;

void*ld(FILE*f,Lump*l){void*p=malloc(l->l);fseek(f,l->o,0);fread(p,1,l->l,f);return p;}

BSP loadBSP(const char*path){
    FILE*f=fopen(path,"rb");if(!f){fprintf(stderr,"Failed to open %s\n",path);exit(1);}
    BSPHdr h;fread(&h,sizeof(h),1,f);
    if(memcmp(h.sig,"IBSP",4)||h.ver!=46){fprintf(stderr,"Invalid BSP\n");exit(1);}

    BSP b={0};
    if(h.lm[0].l){b.ent=ld(f,&h.lm[0]);}
    if(h.lm[1].l){b.sh=ld(f,&h.lm[1]);b.ns=h.lm[1].l/sizeof(BSPShader);}
    if(h.lm[2].l){b.pl=ld(f,&h.lm[2]);b.np=h.lm[2].l/sizeof(BSPPlane);}
    if(h.lm[3].l){b.nd=ld(f,&h.lm[3]);b.nn=h.lm[3].l/sizeof(BSPNode);}
    if(h.lm[4].l){b.lf=ld(f,&h.lm[4]);b.nl=h.lm[4].l/sizeof(BSPLeaf);}
    if(h.lm[5].l){b.lffc=ld(f,&h.lm[5]);b.nlf=h.lm[5].l/sizeof(int);}
    if(h.lm[10].l){b.verts=ld(f,&h.lm[10]);b.nv=h.lm[10].l/sizeof(BSPVert);}
    if(h.lm[11].l){b.idx=ld(f,&h.lm[11]);b.ni=h.lm[11].l/sizeof(uint32_t);}
    if(h.lm[14].l){b.lm=ld(f,&h.lm[14]);b.nlm=h.lm[14].l/(128*128*3);}

    fclose(f);
    return b;
}

typedef struct{uint8_t*px;int w,h,ch;}Img;

Img loadTGA(const char*path){
    FILE*f=fopen(path,"rb");if(!f){fprintf(stderr,"Failed to open %s\n",path);exit(1);}
    uint8_t hdr[18];fread(hdr,1,18,f);
    int w=hdr[12]|(hdr[13]<<8);
    int h=hdr[14]|(hdr[15]<<8);
    int bpp=hdr[16];
    int ch=bpp/8;
    Img img={malloc(w*h*ch),w,h,ch};
    fread(img.px,1,w*h*ch,f);
    fclose(f);
    for(int i=0;i<w*h;i++){
        uint8_t t=img.px[i*ch];
        img.px[i*ch]=img.px[i*ch+2];
        img.px[i*ch+2]=t;
    }
    return img;
}

void saveTGA(const char*path,uint8_t*px,int w,int h){
    FILE*f=fopen(path,"wb");
    uint8_t hdr[18]={0,0,2,0,0,0,0,0,0,0,0,0,w&255,w>>8,h&255,h>>8,24,0};
    fwrite(hdr,1,18,f);
    for(int i=0;i<w*h;i++){
        fwrite(&px[i*3+2],1,1,f);
        fwrite(&px[i*3+1],1,1,f);
        fwrite(&px[i*3+0],1,1,f);
    }
    fclose(f);
}

#ifdef TEST_BSP
int main(int argc,char**argv){
    if(argc<2){fprintf(stderr,"Usage: %s <map.bsp>\n",argv[0]);return 1;}

    BSP b=loadBSP(argv[1]);
    printf("BSP loaded: %s\n",argv[1]);
    printf("  Vertices: %u\n",b.nv);
    printf("  Indices: %u (triangles: %u)\n",b.ni,b.ni/3);
    printf("  Planes: %u\n",b.np);
    printf("  Nodes: %u\n",b.nn);
    printf("  Leaves: %u\n",b.nl);
    printf("  Lightmaps: %u\n",b.nlm);
    printf("  Shaders: %u\n",b.ns);

    if(b.nv>0){
        v3 mn=b.verts[0].p,mx=b.verts[0].p;
        for(uint32_t i=1;i<b.nv;i++){
            if(b.verts[i].p.x<mn.x)mn.x=b.verts[i].p.x;
            if(b.verts[i].p.y<mn.y)mn.y=b.verts[i].p.y;
            if(b.verts[i].p.z<mn.z)mn.z=b.verts[i].p.z;
            if(b.verts[i].p.x>mx.x)mx.x=b.verts[i].p.x;
            if(b.verts[i].p.y>mx.y)mx.y=b.verts[i].p.y;
            if(b.verts[i].p.z>mx.z)mx.z=b.verts[i].p.z;
        }
        printf("  Bounds: (%.1f,%.1f,%.1f) - (%.1f,%.1f,%.1f)\n",
               mn.x,mn.y,mn.z,mx.x,mx.y,mx.z);
    }

    if(b.ent){
        char*e=b.ent;
        int nent=0;
        while(*e){if(*e=='{')nent++;e++;}
        printf("  Entities: %d\n",nent);
    }

    return 0;
}
#endif

#ifdef TEST_TGA
int main(int argc,char**argv){
    if(argc<2){fprintf(stderr,"Usage: %s <image.tga>\n",argv[0]);return 1;}

    Img img=loadTGA(argv[1]);
    printf("TGA loaded: %s\n",argv[1]);
    printf("  Size: %d×%d\n",img.w,img.h);
    printf("  Channels: %d\n",img.ch);
    if(img.ch>=3){
        printf("  First pixel: RGB(%d,%d,%d)\n",img.px[0],img.px[1],img.px[2]);
    }

    uint8_t*test=malloc(256*256*3);
    for(int y=0;y<256;y++){
        for(int x=0;x<256;x++){
            int i=(y*256+x)*3;
            test[i+0]=x;
            test[i+1]=y;
            test[i+2]=(x^y);
        }
    }
    saveTGA("test_pattern.tga",test,256,256);
    printf("Generated test_pattern.tga (256×256)\n");

    free(img.px);
    free(test);
    return 0;
}
#endif

#ifdef SOFT_RT
typedef struct{v3 o,d;float tmin,tmax;}Ray;
typedef struct{int hit;float t,tmax;v3 n;int tri;}Hit;

Hit rayTri(Ray r,v3 v0,v3 v1,v3 v2){
    Hit h={0};
    v3 e1=v3sub(v1,v0),e2=v3sub(v2,v0);
    v3 pvec=v3cross(r.d,e2);
    float det=v3dot(e1,pvec);
    if(fabsf(det)<1e-6f)return h;
    float inv=1.f/det;
    v3 tvec=v3sub(r.o,v0);
    float u=v3dot(tvec,pvec)*inv;
    if(u<0.f||u>1.f)return h;
    v3 qvec=v3cross(tvec,e1);
    float v=v3dot(r.d,qvec)*inv;
    if(v<0.f||u+v>1.f)return h;
    float t=v3dot(e2,qvec)*inv;
    if(t<r.tmin||t>r.tmax)return h;
    h.hit=1;h.t=t;
    h.n=v3norm(v3cross(e1,e2));
    return h;
}

Hit traceBSP(BSP*b,Ray r){
    Hit best={0};best.tmax=r.tmax;
    for(uint32_t i=0;i<b->ni;i+=3){
        v3 v0=b->verts[b->idx[i+0]].p;
        v3 v1=b->verts[b->idx[i+1]].p;
        v3 v2=b->verts[b->idx[i+2]].p;
        Hit h=rayTri(r,v0,v1,v2);
        if(h.hit&&h.t<best.tmax){
            best=h;best.tmax=h.t;best.tri=i/3;
        }
    }
    return best;
}

void render(BSP*b,uint8_t*fb,int w,int h,v3 pos,v3 dir,v3 up){
    v3 right=v3norm(v3cross(dir,up));
    up=v3norm(v3cross(right,dir));
    float fov=90.f*M_PI/180.f;
    float asp=(float)w/(float)h;

    #pragma omp parallel for schedule(dynamic,1)
    for(int y=0;y<h;y++){
        for(int x=0;x<w;x++){
            float u=(2.f*x/(float)w-1.f)*tanf(fov/2.f)*asp;
            float v=(1.f-2.f*y/(float)h)*tanf(fov/2.f);

            Ray r;
            r.o=pos;
            r.d=v3norm(v3add(v3add(dir,v3mul(right,u)),v3mul(up,v)));
            r.tmin=0.1f;r.tmax=10000.f;

            Hit h=traceBSP(b,r);

            int idx=(y*w+x)*3;
            if(h.hit){
                v3 light=v3norm(V3(1,1,2));
                float diff=fmaxf(0.f,v3dot(h.n,light))*0.8f+0.2f;
                fb[idx+0]=(uint8_t)(diff*255.f);
                fb[idx+1]=(uint8_t)(diff*255.f);
                fb[idx+2]=(uint8_t)(diff*255.f);
            }else{
                fb[idx+0]=50;
                fb[idx+1]=50;
                fb[idx+2]=80;
            }
        }
        if(y%10==0)printf("\rRendering: %d%%",(y*100)/h);fflush(stdout);
    }
    printf("\rRendering: 100%%\n");
}

int main(int argc,char**argv){
    if(argc<3){
        fprintf(stderr,"Usage: %s <map.bsp> <out.tga> [x y z] [dx dy dz]\n",argv[0]);
        fprintf(stderr,"Example: %s assets/maps/aggressor.bsp test.tga\n",argv[0]);
        return 1;
    }

    BSP b=loadBSP(argv[1]);
    printf("Loaded BSP: %u verts, %u tris\n",b.nv,b.ni/3);

    v3 pos=V3(0,0,100);
    v3 dir=V3(1,0,0);

    if(argc>=6){
        pos.x=atof(argv[3]);
        pos.y=atof(argv[4]);
        pos.z=atof(argv[5]);
    }
    if(argc>=9){
        dir.x=atof(argv[6]);
        dir.y=atof(argv[7]);
        dir.z=atof(argv[8]);
        dir=v3norm(dir);
    }

    int w=800,h=600;
    uint8_t*fb=malloc(w*h*3);

    printf("Rendering %dx%d from (%.1f,%.1f,%.1f) dir (%.2f,%.2f,%.2f)\n",
           w,h,pos.x,pos.y,pos.z,dir.x,dir.y,dir.z);

    render(&b,fb,w,h,pos,dir,V3(0,0,1));

    saveTGA(argv[2],fb,w,h);
    printf("Saved to %s\n",argv[2]);

    free(fb);
    return 0;
}
#endif

#ifdef VK_RT
#include<SDL2/SDL.h>
#include<SDL2/SDL_vulkan.h>
#include<vulkan/vulkan.h>

#define VK(x) do{VkResult r=x;if(r)fprintf(stderr,#x" = %d\n",r),exit(1);}while(0)

typedef struct{VkBuffer b;VkDeviceMemory m;VkDeviceAddress a;}Buf;

Buf mkBuf(VkDevice d,VkPhysicalDevice p,VkDeviceSize sz,VkBufferUsageFlags u,VkMemoryPropertyFlags f){
Buf b;VkBufferCreateInfo bi={VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,0,0,sz,u};
VK(vkCreateBuffer(d,&bi,0,&b.b));
VkMemoryRequirements mr;vkGetBufferMemoryRequirements(d,b.b,&mr);
VkPhysicalDeviceMemoryProperties mp;vkGetPhysicalDeviceMemoryProperties(p,&mp);
uint32_t mt=0;for(uint32_t i=0;i<mp.memoryTypeCount;i++)
if((mr.memoryTypeBits&(1<<i))&&(mp.memoryTypes[i].propertyFlags&f)==f){mt=i;break;}
VkMemoryAllocateFlagsInfo fi={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,0,VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT};
VkMemoryAllocateInfo ai={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,&fi,mr.size,mt};
VK(vkAllocateMemory(d,&ai,0,&b.m));
VK(vkBindBufferMemory(d,b.b,b.m,0));
VkBufferDeviceAddressInfo da={VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,0,b.b};
b.a=vkGetBufferDeviceAddress(d,&da);
return b;
}

void upBuf(VkDevice d,Buf b,void*src,size_t sz){
void*dst;VK(vkMapMemory(d,b.m,0,sz,0,&dst));memcpy(dst,src,sz);vkUnmapMemory(d,b.m);
}

VkShaderModule ldSh(VkDevice d,const char*p){
FILE*f=fopen(p,"rb");fseek(f,0,SEEK_END);size_t sz=ftell(f);rewind(f);
uint32_t*code=malloc(sz);fread(code,1,sz,f);fclose(f);
VkShaderModuleCreateInfo ci={VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,0,0,sz,code};
VkShaderModule sm;VK(vkCreateShaderModule(d,&ci,0,&sm));free(code);return sm;
}

int main(int argc,char**argv){
if(argc<2){fprintf(stderr,"Usage: %s <map.bsp> [out.tga]\n",argv[0]);return 1;}

BSP bsp=loadBSP(argv[1]);
printf("Loaded %u verts, %u tris\n",bsp.nv,bsp.ni/3);

SDL_Init(SDL_INIT_VIDEO);
SDL_Window*win=SDL_CreateWindow("Q3RT",0,0,1280,720,SDL_WINDOW_VULKAN);

uint32_t extCnt;SDL_Vulkan_GetInstanceExtensions(win,&extCnt,0);
const char**exts=malloc(sizeof(char*)*extCnt);
SDL_Vulkan_GetInstanceExtensions(win,&extCnt,exts);

VkApplicationInfo ai={VK_STRUCTURE_TYPE_APPLICATION_INFO,0,"Q3RT",1,"Q3RT",1,VK_API_VERSION_1_3};
VkInstanceCreateInfo ici={VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,0,0,&ai,0,0,extCnt,exts};
VkInstance inst;VK(vkCreateInstance(&ici,0,&inst));

VkPhysicalDevice pd;uint32_t pdCnt=1;vkEnumeratePhysicalDevices(inst,&pdCnt,&pd);

uint32_t qf=0;
VkPhysicalDeviceFeatures2 f2={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
VkPhysicalDeviceVulkan12Features f12={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
VkPhysicalDeviceAccelerationStructureFeaturesKHR asf={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
VkPhysicalDeviceRayQueryFeaturesKHR rqf={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
f2.pNext=&f12;f12.pNext=&asf;asf.pNext=&rqf;
vkGetPhysicalDeviceFeatures2(pd,&f2);
f12.bufferDeviceAddress=VK_TRUE;asf.accelerationStructure=VK_TRUE;rqf.rayQuery=VK_TRUE;

float qp=1.f;VkDeviceQueueCreateInfo qci={VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,0,0,qf,1,&qp};
const char*devExts[]={"VK_KHR_swapchain","VK_KHR_acceleration_structure","VK_KHR_ray_query",
"VK_KHR_deferred_host_operations","VK_KHR_buffer_device_address"};
VkDeviceCreateInfo dci={VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,&f2,0,1,&qci,0,0,5,devExts,0};
VkDevice dev;VK(vkCreateDevice(pd,&dci,0,&dev));

VkQueue q;vkGetDeviceQueue(dev,qf,0,&q);

VkSurfaceKHR surf;SDL_Vulkan_CreateSurface(win,inst,&surf);

VkSwapchainCreateInfoKHR sci={VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,0,0,surf,2,VK_FORMAT_B8G8R8A8_UNORM,
VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,{1280,720},1,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
VK_SHARING_MODE_EXCLUSIVE,0,0,VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
VK_PRESENT_MODE_FIFO_KHR,VK_TRUE,0};
VkSwapchainKHR sc;VK(vkCreateSwapchainKHR(dev,&sci,0,&sc));

uint32_t imgCnt;vkGetSwapchainImagesKHR(dev,sc,&imgCnt,0);
VkImage scImgs[8];vkGetSwapchainImagesKHR(dev,sc,&imgCnt,scImgs);
VkImageView scViews[8];
for(uint32_t i=0;i<imgCnt;i++){
VkImageViewCreateInfo vci={VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,0,0,scImgs[i],VK_IMAGE_VIEW_TYPE_2D,
VK_FORMAT_B8G8R8A8_UNORM,{},{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}};
VK(vkCreateImageView(dev,&vci,0,&scViews[i]));
}

Buf vb=mkBuf(dev,pd,sizeof(BSPVert)*bsp.nv,
VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
upBuf(dev,vb,bsp.verts,sizeof(BSPVert)*bsp.nv);

Buf ib=mkBuf(dev,pd,sizeof(uint32_t)*bsp.ni,
VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
upBuf(dev,ib,bsp.idx,sizeof(uint32_t)*bsp.ni);

PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR=
(PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(dev,"vkGetAccelerationStructureBuildSizesKHR");
PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR=
(PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(dev,"vkCreateAccelerationStructureKHR");
PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR=
(PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(dev,"vkCmdBuildAccelerationStructuresKHR");
PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR=
(PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(dev,"vkGetAccelerationStructureDeviceAddressKHR");

VkAccelerationStructureGeometryKHR geom={VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,0,
VK_GEOMETRY_TYPE_TRIANGLES_KHR,{.triangles={VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,0,
VK_FORMAT_R32G32B32_SFLOAT,{.deviceAddress=vb.a},sizeof(BSPVert),bsp.nv-1,VK_INDEX_TYPE_UINT32,{.deviceAddress=ib.a},
{.deviceAddress=0}}},VK_GEOMETRY_OPAQUE_BIT_KHR};
VkAccelerationStructureBuildGeometryInfoKHR bgi={VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,0,
VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,0,0,1,&geom};
VkAccelerationStructureBuildSizesInfoKHR szi={VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
uint32_t primCnt=bsp.ni/3;
vkGetAccelerationStructureBuildSizesKHR(dev,VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,&bgi,&primCnt,&szi);

Buf asBuf=mkBuf(dev,pd,szi.accelerationStructureSize,VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR|
VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
VkAccelerationStructureCreateInfoKHR asci={VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,0,0,asBuf.b,0,
szi.accelerationStructureSize,VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,0};
VkAccelerationStructureKHR blas;VK(vkCreateAccelerationStructureKHR(dev,&asci,0,&blas));

Buf scratchBuf=mkBuf(dev,pd,szi.buildScratchSize,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
bgi.dstAccelerationStructure=blas;
bgi.scratchData.deviceAddress=scratchBuf.a;

VkCommandPoolCreateInfo pci={VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,0,0,qf};
VkCommandPool pool;VK(vkCreateCommandPool(dev,&pci,0,&pool));
VkCommandBufferAllocateInfo cai={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,0,pool,VK_COMMAND_BUFFER_LEVEL_PRIMARY,1};
VkCommandBuffer cmd;VK(vkAllocateCommandBuffers(dev,&cai,&cmd));

VkCommandBufferBeginInfo cbi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,0,VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
VK(vkBeginCommandBuffer(cmd,&cbi));
VkAccelerationStructureBuildRangeInfoKHR bri={primCnt,0,0,0};
VkAccelerationStructureBuildRangeInfoKHR*bris=&bri;
vkCmdBuildAccelerationStructuresKHR(cmd,1,&bgi,&bris);
VK(vkEndCommandBuffer(cmd));

VkSubmitInfo si={VK_STRUCTURE_TYPE_SUBMIT_INFO,0,0,0,0,1,&cmd};
VK(vkQueueSubmit(q,1,&si,0));
VK(vkQueueWaitIdle(q));

VkAccelerationStructureDeviceAddressInfoKHR dai={VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,0,blas};
VkDeviceAddress blasAddr=vkGetAccelerationStructureDeviceAddressKHR(dev,&dai);

VkAccelerationStructureInstanceKHR inst={{1,0,0,0,0,1,0,0,0,0,1,0},0,0xff,0,VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
blasAddr};
Buf instBuf=mkBuf(dev,pd,sizeof(inst),VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR|
VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
upBuf(dev,instBuf,&inst,sizeof(inst));

VkAccelerationStructureGeometryKHR tlGeom={VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,0,VK_GEOMETRY_TYPE_INSTANCES_KHR,
{.instances={VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,0,VK_FALSE,{.deviceAddress=instBuf.a}}},0};
VkAccelerationStructureBuildGeometryInfoKHR tlBgi={VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,0,
VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,0,0,1,&tlGeom};
uint32_t instCnt=1;
vkGetAccelerationStructureBuildSizesKHR(dev,VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,&tlBgi,&instCnt,&szi);

Buf tlasBuf=mkBuf(dev,pd,szi.accelerationStructureSize,VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
VkAccelerationStructureCreateInfoKHR tlAsci={VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,0,0,tlasBuf.b,0,
szi.accelerationStructureSize,VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,0};
VkAccelerationStructureKHR tlas;VK(vkCreateAccelerationStructureKHR(dev,&tlAsci,0,&tlas));

Buf tlScratch=mkBuf(dev,pd,szi.buildScratchSize,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
tlBgi.dstAccelerationStructure=tlas;
tlBgi.scratchData.deviceAddress=tlScratch.a;

VK(vkResetCommandBuffer(cmd,0));
VK(vkBeginCommandBuffer(cmd,&cbi));
VkAccelerationStructureBuildRangeInfoKHR tlBri={1,0,0,0};
VkAccelerationStructureBuildRangeInfoKHR*tlBris=&tlBri;
vkCmdBuildAccelerationStructuresKHR(cmd,1,&tlBgi,&tlBris);
VK(vkEndCommandBuffer(cmd));
VK(vkQueueSubmit(q,1,&si,0));
VK(vkQueueWaitIdle(q));

printf("Built BLAS (%u tris) + TLAS\n",primCnt);

SDL_DestroyWindow(win);
SDL_Quit();
return 0;
}
#endif
