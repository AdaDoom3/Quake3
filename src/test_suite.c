// Quake3 Raytracing Engine - Comprehensive Test Suite
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#define W 1024
#define H 768
#define PI 3.14159265359

// Base shaders
const char *vs = "#version 330 core\n"
"layout(location=0)in vec2 p;out vec2 uv;void main(){gl_Position=vec4(p,0,1);uv=p*.5+.5;}";

// Multiple test scenes with different complexity levels
const char *fs_scenes[] = {
    // Scene 0: Basic geometric primitives test
    "#version 330 core\n"
    "uniform vec2 R;uniform float T;uniform int S;in vec2 uv;out vec4 C;\n"
    "float h(float n){return fract(sin(n)*43758.5453);}\n"
    "float sBox(vec3 p,vec3 b){vec3 q=abs(p)-b;return length(max(q,0.))+min(max(q.x,max(q.y,q.z)),0.);}\n"
    "float sSph(vec3 p,float r){return length(p)-r;}\n"
    "float sTor(vec3 p,vec2 t){vec2 q=vec2(length(p.xz)-t.x,p.y);return length(q)-t.y;}\n"
    "float map(vec3 p){return min(min(sBox(p-vec3(-2,0,0),vec3(1)),sSph(p-vec3(2,0,0),1.)),sTor(p-vec3(0,0,0),vec2(1.5,.3)));}\n"
    "vec3 norm(vec3 p){vec2 e=vec2(.001,0);return normalize(vec3(map(p+e.xyy)-map(p-e.xyy),map(p+e.yxy)-map(p-e.yxy),map(p+e.yyx)-map(p-e.yyx)));}\n"
    "float march(vec3 o,vec3 d){float t=0.;for(int i=0;i<96;i++){float h=map(o+d*t);if(h<.001||t>50.)break;t+=h;}return t;}\n"
    "void main(){vec2 p=(uv-.5)*vec2(R.x/R.y,1.)*2.;vec3 ro=vec3(cos(T*.5)*6.,sin(T*.3)*2.,sin(T*.5)*6.),ta=vec3(0),"
    "f=normalize(ta-ro),r=normalize(cross(vec3(0,1,0),f)),u=cross(f,r);vec3 rd=normalize(p.x*r+p.y*u+2.*f);"
    "float t=march(ro,rd);vec3 col=vec3(.05,.1,.15);if(t<50.){vec3 pos=ro+rd*t,nor=norm(pos);"
    "vec3 lig=normalize(vec3(.5,1.,.3));float dif=clamp(dot(nor,lig),0.,1.);col=vec3(.7,.6,.5)*dif;}"
    "col=pow(col,vec3(.4545));C=vec4(col,1);}",

    // Scene 1: Complex room with domain repetition
    "#version 330 core\n"
    "uniform vec2 R;uniform float T;uniform int S;in vec2 uv;out vec4 C;\n"
    "float h(float n){return fract(sin(n)*43758.5453);}\n"
    "vec3 h3(vec3 p){p=fract(p*vec3(.1031,.1030,.0973));p+=dot(p,p.yxz+33.33);return fract((p.xxy+p.yxx)*p.zyx);}\n"
    "float n(vec3 x){vec3 p=floor(x),f=fract(x);f=f*f*(3.-2.*f);float n=p.x+p.y*157.+113.*p.z;"
    "return mix(mix(mix(h(n),h(n+1.),f.x),mix(h(n+157.),h(n+158.),f.x),f.y),"
    "mix(mix(h(n+113.),h(n+114.),f.x),mix(h(n+270.),h(n+271.),f.x),f.y),f.z);}\n"
    "float sBox(vec3 p,vec3 b){vec3 q=abs(p)-b;return length(max(q,0.))+min(max(q.x,max(q.y,q.z)),0.);}\n"
    "float sSph(vec3 p,float r){return length(p)-r;}\n"
    "float sCap(vec3 p,vec3 a,vec3 b,float r){vec3 pa=p-a,ba=b-a;float h=clamp(dot(pa,ba)/dot(ba,ba),0.,1.);return length(pa-ba*h)-r;}\n"
    "float smin(float a,float b,float k){float h=clamp(.5+.5*(b-a)/k,0.,1.);return mix(b,a,h)-k*h*(1.-h);}\n"
    "float map(vec3 p){vec3 q=p;q.xz=fract(q.xz+.5)-.5;float d=sBox(p-vec3(0,-2,0),vec3(8,1,8));"
    "d=min(d,sBox(p-vec3(0,6,0),vec3(8,1,8)));d=min(d,sBox(p-vec3(-8,2,0),vec3(1,5,8)));"
    "d=min(d,sBox(p-vec3(8,2,0),vec3(1,5,8)));d=min(d,sBox(p-vec3(0,2,8),vec3(8,5,1)));"
    "float pillar=sBox(q-vec3(0,0,0),vec3(.3,4,.3));d=smin(d,pillar,.3);"
    "float torch=sCap(q-vec3(0,1,0),vec3(0,0,0),vec3(0,.8,0),.1);d=min(d,torch);return d;}\n"
    "vec3 norm(vec3 p){vec2 e=vec2(.001,0);return normalize(vec3(map(p+e.xyy)-map(p-e.xyy),map(p+e.yxy)-map(p-e.yxy),map(p+e.yyx)-map(p-e.yyx)));}\n"
    "float march(vec3 o,vec3 d){float t=0.;for(int i=0;i<64;i++){float h=map(o+d*t);if(h<.001||t>50.)break;t+=h;}return t;}\n"
    "float ao(vec3 p,vec3 n){float o=0.,s=1.;for(int i=0;i<5;i++){float h=.01+.12*float(i)/4.;float d=map(p+h*n);o+=s*(h-d);s*.=.95;}return clamp(1.-3.*o,0.,1.);}\n"
    "void main(){vec2 p=(uv-.5)*vec2(R.x/R.y,1.)*2.;vec3 ro=vec3(cos(T*.3)*5.,2.+sin(T*.5),sin(T*.3)*5.),ta=vec3(0,2,0),"
    "f=normalize(ta-ro),r=normalize(cross(vec3(0,1,0),f)),u=cross(f,r);vec3 rd=normalize(p.x*r+p.y*u+2.*f);"
    "float t=march(ro,rd);vec3 col=vec3(.1,.15,.2);if(t<50.){vec3 pos=ro+rd*t,nor=norm(pos);"
    "vec3 lig=normalize(vec3(.5,1.,.3)),hal=normalize(lig-rd);float dif=clamp(dot(nor,lig),0.,1.),"
    "spe=pow(clamp(dot(nor,hal),0.,1.),16.),occ=ao(pos,nor),fre=pow(clamp(1.+dot(nor,rd),0.,1.),2.);"
    "float tex=n(pos*4.)*.5+.5;col=vec3(.6,.5,.4)*tex;col*=dif*occ;col+=spe*.5*occ;col+=fre*.2*occ;"
    "col=mix(col,vec3(.1,.15,.2),1.-exp(-.01*t*t));}col=pow(col,vec3(.4545));C=vec4(col,1);}",

    // Scene 2: Stress test - many objects with smooth blending
    "#version 330 core\n"
    "uniform vec2 R;uniform float T;uniform int S;in vec2 uv;out vec4 C;\n"
    "float sSph(vec3 p,float r){return length(p)-r;}\n"
    "float sBox(vec3 p,vec3 b){vec3 q=abs(p)-b;return length(max(q,0.))+min(max(q.x,max(q.y,q.z)),0.);}\n"
    "float smin(float a,float b,float k){float h=clamp(.5+.5*(b-a)/k,0.,1.);return mix(b,a,h)-k*h*(1.-h);}\n"
    "float map(vec3 p){float d=1e10;for(int i=0;i<8;i++){float a=float(i)*3.14159*.25;"
    "vec3 q=p-vec3(cos(a+T)*3.,sin(T+float(i)),sin(a+T)*3.);d=smin(d,sSph(q,.5),.5);}"
    "d=smin(d,sBox(p,vec3(2)),.3);return d;}\n"
    "vec3 norm(vec3 p){vec2 e=vec2(.001,0);return normalize(vec3(map(p+e.xyy)-map(p-e.xyy),map(p+e.yxy)-map(p-e.yxy),map(p+e.yyx)-map(p-e.yyx)));}\n"
    "float march(vec3 o,vec3 d){float t=0.;for(int i=0;i<128;i++){float h=map(o+d*t);if(h<.001||t>50.)break;t+=h;}return t;}\n"
    "void main(){vec2 p=(uv-.5)*vec2(R.x/R.y,1.)*2.;vec3 ro=vec3(cos(T*.4)*8.,3.,sin(T*.4)*8.),ta=vec3(0,0,0),"
    "f=normalize(ta-ro),r=normalize(cross(vec3(0,1,0),f)),u=cross(f,r);vec3 rd=normalize(p.x*r+p.y*u+2.*f);"
    "float t=march(ro,rd);vec3 col=vec3(.02,.05,.1);if(t<50.){vec3 pos=ro+rd*t,nor=norm(pos);"
    "vec3 lig=normalize(vec3(.5,1.,.3));float dif=clamp(dot(nor,lig),0.,1.);"
    "col=mix(vec3(.8,.3,.2),vec3(.2,.3,.8),sin(pos.y*2.)*.5+.5)*dif;}"
    "col=pow(col,vec3(.4545));C=vec4(col,1);}",

    // Scene 3: Fractal-like pattern with domain folding
    "#version 330 core\n"
    "uniform vec2 R;uniform float T;uniform int S;in vec2 uv;out vec4 C;\n"
    "float sBox(vec3 p,vec3 b){vec3 q=abs(p)-b;return length(max(q,0.))+min(max(q.x,max(q.y,q.z)),0.);}\n"
    "float sSph(vec3 p,float r){return length(p)-r;}\n"
    "float map(vec3 p){p=abs(p);if(p.x<p.y)p.xy=p.yx;if(p.x<p.z)p.xz=p.zx;if(p.y<p.z)p.yz=p.zy;"
    "p.xyz-=vec3(1);float d=sBox(p,vec3(.5));for(int i=0;i<3;i++){p=abs(p)-vec3(.5);d=min(d,sSph(p,.3));}return d;}\n"
    "vec3 norm(vec3 p){vec2 e=vec2(.001,0);return normalize(vec3(map(p+e.xyy)-map(p-e.xyy),map(p+e.yxy)-map(p-e.yxy),map(p+e.yyx)-map(p-e.yyx)));}\n"
    "float march(vec3 o,vec3 d){float t=0.;for(int i=0;i<96;i++){float h=map(o+d*t);if(h<.001||t>50.)break;t+=h;}return t;}\n"
    "void main(){vec2 p=(uv-.5)*vec2(R.x/R.y,1.)*2.;vec3 ro=vec3(cos(T*.6)*4.,sin(T*.3)*2.,sin(T*.6)*4.),ta=vec3(0),"
    "f=normalize(ta-ro),r=normalize(cross(vec3(0,1,0),f)),u=cross(f,r);vec3 rd=normalize(p.x*r+p.y*u+1.5*f);"
    "float t=march(ro,rd);vec3 col=vec3(0);if(t<50.){vec3 pos=ro+rd*t,nor=norm(pos);"
    "float fres=pow(1.-abs(dot(nor,rd)),3.);col=mix(vec3(.1,.3,.5),vec3(.9,.7,.3),fres);}"
    "col=pow(col,vec3(.4545));C=vec4(col,1);}"
};

const char *scene_names[] = {
    "Basic Primitives Test",
    "Complex Room (Original)",
    "Stress Test - Smooth Blending",
    "Fractal Domain Folding"
};

const int num_scenes = 4;

GLuint mkShd(GLenum t, const char *s) {
    GLuint S = glCreateShader(t);
    glShaderSource(S, 1, &s, NULL);
    glCompileShader(S);
    GLint ok;
    glGetShaderiv(S, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(S, 1024, NULL, log);
        printf("Shader compile error:\n%s\n", log);
    }
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
}

// Performance benchmark
typedef struct {
    int scene;
    float time;
    int frames;
    double avg_fps;
    double min_fps;
    double max_fps;
} BenchResult;

BenchResult benchmark_scene(GLFWwindow *w, GLuint prg, int scene_id, float duration) {
    BenchResult res = {scene_id, 0, 0, 0, 1e10, 0};

    GLint locR = glGetUniformLocation(prg, "R");
    GLint locT = glGetUniformLocation(prg, "T");
    GLint locS = glGetUniformLocation(prg, "S");

    GLuint vao, vbo;
    float verts[] = {-1, -1, 1, -1, -1, 1, 1, -1, 1, 1, -1, 1};
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    double start_time = glfwGetTime();
    double last_time = start_time;
    int frames = 0;

    while (glfwGetTime() - start_time < duration) {
        double current = glfwGetTime();
        float t = (float)(current - start_time);

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prg);
        glUniform2f(locR, (float)W, (float)H);
        glUniform1f(locT, t);
        glUniform1i(locS, scene_id);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glFinish();

        glfwSwapBuffers(w);
        glfwPollEvents();

        double frame_time = current - last_time;
        double fps = 1.0 / frame_time;
        if (fps < res.min_fps) res.min_fps = fps;
        if (fps > res.max_fps) res.max_fps = fps;

        last_time = current;
        frames++;
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);

    res.frames = frames;
    res.time = duration;
    res.avg_fps = frames / duration;

    return res;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow *w = glfwCreateWindow(W, H, "Test Suite", NULL, NULL);
    glfwMakeContextCurrent(w);

    // Setup VAO
    GLuint vao, vbo;
    float verts[] = {-1, -1, 1, -1, -1, 1, 1, -1, 1, 1, -1, 1};
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    printf("\n");
    printf("================================================================================\n");
    printf("QUAKE3 RAYTRACING ENGINE - COMPREHENSIVE TEST SUITE\n");
    printf("================================================================================\n\n");

    // Test each scene
    for (int scene = 0; scene < num_scenes; scene++) {
        printf("Scene %d: %s\n", scene, scene_names[scene]);
        printf("────────────────────────────────────────────────────────────────────────────────\n");

        GLuint prg = mkPrg(vs, fs_scenes[scene]);
        GLint locR = glGetUniformLocation(prg, "R");
        GLint locT = glGetUniformLocation(prg, "T");
        GLint locS = glGetUniformLocation(prg, "S");

        // Generate screenshots at multiple time points
        float times[] = {0.0, 1.5, 3.0, 5.0, 7.5};
        char fn[128];

        for (int t = 0; t < 5; t++) {
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(prg);
            glUniform2f(locR, (float)W, (float)H);
            glUniform1f(locT, times[t]);
            glUniform1i(locS, scene);
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glFinish();

            sprintf(fn, "tests/scene%d_t%.1f.ppm", scene, times[t]);
            save_ppm(fn, W, H);
            printf("  ✓ Screenshot: %s (t=%.1fs)\n", fn, times[t]);
        }

        // Performance benchmark
        printf("  Running performance benchmark (3 seconds)...\n");
        BenchResult bench = benchmark_scene(w, prg, scene, 3.0);
        printf("  ✓ Performance: %.1f FPS avg (min: %.1f, max: %.1f) - %d frames\n",
               bench.avg_fps, bench.min_fps, bench.max_fps, bench.frames);

        glDeleteProgram(prg);
        printf("\n");
    }

    printf("================================================================================\n");
    printf("TEST SUITE COMPLETE\n");
    printf("Total scenes tested: %d\n", num_scenes);
    printf("Total screenshots: %d\n", num_scenes * 5);
    printf("Output directory: tests/\n");
    printf("================================================================================\n\n");

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glfwTerminate();
    return 0;
}
