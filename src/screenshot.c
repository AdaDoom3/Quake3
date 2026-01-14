// Quake3 Raytracing Engine - Screenshot version
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#define W 1024
#define H 768
#define PI 3.14159265359

// Shader sources (code-golfed)
const char *vs = "#version 330 core\n"
"layout(location=0)in vec2 p;out vec2 uv;void main(){gl_Position=vec4(p,0,1);uv=p*.5+.5;}";

const char *fs = "#version 330 core\n"
"uniform vec2 R;uniform float T;in vec2 uv;out vec4 C;\n"
// Hash function (Dave Hoskins)
"float h(float n){return fract(sin(n)*43758.5453);}\n"
"vec3 h3(vec3 p){p=fract(p*vec3(.1031,.1030,.0973));p+=dot(p,p.yxz+33.33);"
"return fract((p.xxy+p.yxx)*p.zyx);}\n"
// Noise
"float n(vec3 x){vec3 p=floor(x),f=fract(x);f=f*f*(3.-2.*f);"
"float n=p.x+p.y*157.+113.*p.z;return mix(mix(mix(h(n),h(n+1.),f.x),"
"mix(h(n+157.),h(n+158.),f.x),f.y),mix(mix(h(n+113.),h(n+114.),f.x),"
"mix(h(n+270.),h(n+271.),f.x),f.y),f.z);}\n"
// SDF primitives (Inigo Quilez style)
"float sBox(vec3 p,vec3 b){vec3 q=abs(p)-b;return length(max(q,0.))+min(max(q.x,max(q.y,q.z)),0.);}\n"
"float sSph(vec3 p,float r){return length(p)-r;}\n"
"float sTor(vec3 p,vec2 t){vec2 q=vec2(length(p.xz)-t.x,p.y);return length(q)-t.y;}\n"
"float sCap(vec3 p,vec3 a,vec3 b,float r){vec3 pa=p-a,ba=b-a;"
"float h=clamp(dot(pa,ba)/dot(ba,ba),0.,1.);return length(pa-ba*h)-r;}\n"
// Smooth min (from IQ)
"float smin(float a,float b,float k){float h=clamp(.5+.5*(b-a)/k,0.,1.);"
"return mix(b,a,h)-k*h*(1.-h);}\n"
// Scene SDF (Quake-inspired room)
"float map(vec3 p){"
"vec3 q=p;q.xz=fract(q.xz+.5)-.5;"  // Repeating pattern
"float d=sBox(p-vec3(0,-2,0),vec3(8,1,8));"  // Floor
"d=min(d,sBox(p-vec3(0,6,0),vec3(8,1,8)));"  // Ceiling
"d=min(d,sBox(p-vec3(-8,2,0),vec3(1,5,8)));"  // Left wall
"d=min(d,sBox(p-vec3(8,2,0),vec3(1,5,8)));"  // Right wall
"d=min(d,sBox(p-vec3(0,2,8),vec3(8,5,1)));"  // Back wall
"float pillar=sBox(q-vec3(0,0,0),vec3(.3,4,.3));"  // Pillars
"d=smin(d,pillar,.3);"
"float torch=sCap(q-vec3(0,1,0),vec3(0,0,0),vec3(0,.8,0),.1);"
"d=min(d,torch);"
"return d;}\n"
// Normal calculation
"vec3 norm(vec3 p){vec2 e=vec2(.001,0);return normalize(vec3("
"map(p+e.xyy)-map(p-e.xyy),map(p+e.yxy)-map(p-e.yxy),"
"map(p+e.yyx)-map(p-e.yyx)));}\n"
// Raymarching
"float march(vec3 o,vec3 d){float t=0.;for(int i=0;i<64;i++){"
"float h=map(o+d*t);if(h<.001||t>50.)break;t+=h;}return t;}\n"
// Lighting
"float ao(vec3 p,vec3 n){float o=0.,s=1.;for(int i=0;i<5;i++){"
"float h=.01+.12*float(i)/4.;float d=map(p+h*n);o+=s*(h-d);s*.=.95;}"
"return clamp(1.-3.*o,0.,1.);}\n"
// Main
"void main(){"
"vec2 p=(uv-.5)*vec2(R.x/R.y,1.)*2.;"
"vec3 ro=vec3(cos(T*.3)*5.,2.+sin(T*.5),sin(T*.3)*5.);"  // Camera orbit
"vec3 ta=vec3(0,2,0),f=normalize(ta-ro),r=normalize(cross(vec3(0,1,0),f)),"
"u=cross(f,r);vec3 rd=normalize(p.x*r+p.y*u+2.*f);"
"float t=march(ro,rd);vec3 col=vec3(.1,.15,.2);"  // Sky color
"if(t<50.){vec3 pos=ro+rd*t,nor=norm(pos);"
// Lighting calculation
"vec3 lig=normalize(vec3(.5,1.,.3)),hal=normalize(lig-rd);"
"float dif=clamp(dot(nor,lig),0.,1.),spe=pow(clamp(dot(nor,hal),0.,1.),16.),"
"occ=ao(pos,nor),fre=pow(clamp(1.+dot(nor,rd),0.,1.),2.);"
// Texture (procedural)
"float tex=n(pos*4.)*.5+.5;"
"col=vec3(.6,.5,.4)*tex;"  // Base color
"col*=dif*occ;"
"col+=spe*.5*occ;"
"col+=fre*.2*occ;"
// Fog
"col=mix(col,vec3(.1,.15,.2),1.-exp(-.01*t*t));}"
"col=pow(col,vec3(.4545));"  // Gamma correction
"C=vec4(col,1);}";

GLuint mkShd(GLenum t, const char *s) {
    GLuint S = glCreateShader(t);
    glShaderSource(S, 1, &s, NULL);
    glCompileShader(S);
    return S;
}

GLuint mkPrg(const char *v, const char *f) {
    GLuint P = glCreateProgram();
    glAttachShader(P, mkShd(GL_VERTEX_SHADER, v));
    glAttachShader(P, mkShd(GL_FRAGMENT_SHADER, f));
    glLinkProgram(P);
    return P;
}

void save_ppm(const char *fn, int w, int h) {
    unsigned char *px = malloc(w * h * 3);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px);
    FILE *fp = fopen(fn, "wb");
    fprintf(fp, "P6\n%d %d\n255\n", w, h);
    for (int i = h - 1; i >= 0; i--)
        fwrite(px + i * w * 3, 3, w, fp);
    fclose(fp);
    free(px);
    printf("Screenshot: %s\n", fn);
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow *w = glfwCreateWindow(W, H, "Screenshot", NULL, NULL);
    glfwMakeContextCurrent(w);

    GLuint vao, vbo;
    float verts[] = {-1, -1, 1, -1, -1, 1, 1, -1, 1, 1, -1, 1};
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    GLuint prg = mkPrg(vs, fs);
    GLint locR = glGetUniformLocation(prg, "R");
    GLint locT = glGetUniformLocation(prg, "T");

    // Render multiple frames at different times
    float times[] = {0.0, 2.0, 5.0, 10.0};
    char fn[64];

    for (int i = 0; i < 4; i++) {
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prg);
        glUniform2f(locR, (float)W, (float)H);
        glUniform1f(locT, times[i]);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glFinish();

        sprintf(fn, "screenshot_%d.ppm", i);
        save_ppm(fn, W, H);
    }

    glfwTerminate();
    return 0;
}
