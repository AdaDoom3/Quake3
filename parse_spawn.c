#include<stdio.h>
#include<string.h>
#include<stdlib.h>

void parseSpawn(char*ent){
    char*p=ent;
    int inEnt=0,inKey=0;
    char cls[64]={0},orig[64]={0},ang[64]={0};
    while(*p){
        if(*p=='{'){inEnt=1;cls[0]=orig[0]=ang[0]=0;}
        else if(*p=='}'&&inEnt){
            if(strstr(cls,"info_player_deathmatch")||strstr(cls,"info_player_start")){
                float x=0,y=0,z=0,yaw=0;
                sscanf(orig,"%f %f %f",&x,&y,&z);
                sscanf(ang,"%f",&yaw);
                float rad=yaw*3.14159f/180.f;
                printf("%.0f %.0f %.0f %.3f %.3f 0\n",x,y,z+40,cosf(rad),sinf(rad));
            }
            inEnt=0;
        }
        else if(inEnt){
            if(*p=='"'){
                char key[64],val[256];
                if(sscanf(p,"\"%63[^\"]\" \"%255[^\"]\"",key,val)==2){
                    if(!strcmp(key,"classname"))strcpy(cls,val);
                    else if(!strcmp(key,"origin"))strcpy(orig,val);
                    else if(!strcmp(key,"angle"))strcpy(ang,val);
                    while(*p&&*p!='\n')p++;
                }
            }
        }
        p++;
    }
}

int main(int argc,char**argv){
    if(argc<2)return 1;
    FILE*f=fopen(argv[1],"rb");
    fseek(f,0,SEEK_END);
    long sz=ftell(f);
    rewind(f);
    char sig[4];int ver;fread(sig,1,4,f);fread(&ver,4,1,f);
    int offs[17],lens[17];
    for(int i=0;i<17;i++)fread(&offs[i],4,1,f),fread(&lens[i],4,1,f);
    if(lens[0]){
        fseek(f,offs[0],0);
        char*ent=malloc(lens[0]+1);
        fread(ent,1,lens[0],f);
        ent[lens[0]]=0;
        parseSpawn(ent);
        free(ent);
    }
    fclose(f);
    return 0;
}
