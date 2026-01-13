#include<stdio.h>
#include<stdint.h>

int main(int argc,char**argv){
    if(argc<2)return 1;
    FILE*f=fopen(argv[1],"rb");
    if(!f)return 1;
    uint8_t h[18];fread(h,1,18,f);
    int w=h[12]|(h[13]<<8),h2=h[14]|(h[15]<<8);
    uint8_t*px=malloc(w*h2*3);
    fread(px,1,w*h2*3,f);
    fclose(f);
    
    int sky=0,geom=0;
    for(int i=0;i<w*h2;i++){
        int r=px[i*3+2],g=px[i*3+1],b=px[i*3];
        if(r==50&&g==50&&b==80)sky++;
        else geom++;
    }
    printf("%s: %dx%d, %.1f%% geometry, %.1f%% sky\n",
           argv[1],w,h2,geom*100.0/(w*h2),sky*100.0/(w*h2));
    free(px);
    return 0;
}
