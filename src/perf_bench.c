// Performance Benchmarking Tool - Detailed Analysis
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#define PI 3.14159265359

typedef struct {
    int width;
    int height;
    int ray_steps;
    float duration;
    int total_frames;
    double avg_fps;
    double min_fps;
    double max_fps;
    double avg_frame_time;
    double total_pixels;
    double pixels_per_sec;
} PerfResult;

const char *vs = "#version 330 core\n"
"layout(location=0)in vec2 p;out vec2 uv;void main(){gl_Position=vec4(p,0,1);uv=p*.5+.5;}";

const char *fs = "#version 330 core\n"
"uniform vec2 R;uniform float T;uniform int M;in vec2 uv;out vec4 C;\n"
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
"float march(vec3 o,vec3 d){float t=0.;for(int i=0;i<M;i++){float h=map(o+d*t);if(h<.001||t>50.)break;t+=h;}return t;}\n"
"float ao(vec3 p,vec3 n){float o=0.,s=1.;for(int i=0;i<5;i++){float h=.01+.12*float(i)/4.;float d=map(p+h*n);o+=s*(h-d);s*.=.95;}return clamp(1.-3.*o,0.,1.);}\n"
"void main(){vec2 p=(uv-.5)*vec2(R.x/R.y,1.)*2.;vec3 ro=vec3(cos(T*.3)*5.,2.+sin(T*.5),sin(T*.3)*5.),ta=vec3(0,2,0),"
"f=normalize(ta-ro),r=normalize(cross(vec3(0,1,0),f)),u=cross(f,r);vec3 rd=normalize(p.x*r+p.y*u+2.*f);"
"float t=march(ro,rd);vec3 col=vec3(.1,.15,.2);if(t<50.){vec3 pos=ro+rd*t,nor=norm(pos);"
"vec3 lig=normalize(vec3(.5,1.,.3)),hal=normalize(lig-rd);float dif=clamp(dot(nor,lig),0.,1.),"
"spe=pow(clamp(dot(nor,hal),0.,1.),16.),occ=ao(pos,nor),fre=pow(clamp(1.+dot(nor,rd),0.,1.),2.);"
"float tex=n(pos*4.)*.5+.5;col=vec3(.6,.5,.4)*tex;col*=dif*occ;col+=spe*.5*occ;col+=fre*.2*occ;"
"col=mix(col,vec3(.1,.15,.2),1.-exp(-.01*t*t));}col=pow(col,vec3(.4545));C=vec4(col,1);}";

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

PerfResult run_benchmark(int w, int h, int steps, float duration) {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow *win = glfwCreateWindow(w, h, "Bench", NULL, NULL);
    glfwMakeContextCurrent(win);

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
    GLint locM = glGetUniformLocation(prg, "M");

    PerfResult res = {w, h, steps, duration, 0, 0, 1e10, 0, 0, 0, 0};

    double start = glfwGetTime();
    double last = start;
    int frames = 0;
    double total_ft = 0;

    while (glfwGetTime() - start < duration) {
        double current = glfwGetTime();
        float t = (float)(current - start);

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prg);
        glUniform2f(locR, (float)w, (float)h);
        glUniform1f(locT, t);
        glUniform1i(locM, steps);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glFinish();

        glfwSwapBuffers(win);
        glfwPollEvents();

        double ft = current - last;
        double fps = 1.0 / ft;
        if (fps < res.min_fps) res.min_fps = fps;
        if (fps > res.max_fps) res.max_fps = fps;
        total_ft += ft;

        last = current;
        frames++;
    }

    res.total_frames = frames;
    res.avg_fps = frames / duration;
    res.avg_frame_time = (total_ft / frames) * 1000.0;
    res.total_pixels = (double)w * h * frames;
    res.pixels_per_sec = res.total_pixels / duration;

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prg);
    glfwTerminate();

    return res;
}

void print_result(PerfResult r, const char *name) {
    printf("  %s:\n", name);
    printf("    Resolution: %dx%d (%d pixels)\n", r.width, r.height, r.width * r.height);
    printf("    Ray Steps:  %d max iterations\n", r.ray_steps);
    printf("    Duration:   %.1f seconds\n", r.duration);
    printf("    Frames:     %d total\n", r.total_frames);
    printf("    Avg FPS:    %.2f\n", r.avg_fps);
    printf("    Min FPS:    %.2f\n", r.min_fps);
    printf("    Max FPS:    %.2f\n", r.max_fps);
    printf("    Frame Time: %.2f ms average\n", r.avg_frame_time);
    printf("    Throughput: %.2f Mpixels/sec\n", r.pixels_per_sec / 1e6);
    printf("\n");
}

int main() {
    printf("\n");
    printf("================================================================================\n");
    printf("PERFORMANCE BENCHMARK - RAYTRACING ENGINE\n");
    printf("================================================================================\n\n");

    printf("Testing different configurations...\n\n");

    // Resolution scaling test
    printf("Resolution Scaling Test:\n");
    printf("────────────────────────────────────────────────────────────────────────────────\n");
    PerfResult r1 = run_benchmark(640, 480, 64, 5.0);
    print_result(r1, "640x480 @ 64 steps");

    PerfResult r2 = run_benchmark(1024, 768, 64, 5.0);
    print_result(r2, "1024x768 @ 64 steps");

    PerfResult r3 = run_benchmark(1920, 1080, 64, 5.0);
    print_result(r3, "1920x1080 @ 64 steps");

    // Ray step scaling test
    printf("Ray Step Complexity Test (1024x768):\n");
    printf("────────────────────────────────────────────────────────────────────────────────\n");
    PerfResult r4 = run_benchmark(1024, 768, 32, 5.0);
    print_result(r4, "32 ray steps");

    PerfResult r5 = run_benchmark(1024, 768, 64, 5.0);
    print_result(r5, "64 ray steps");

    PerfResult r6 = run_benchmark(1024, 768, 128, 5.0);
    print_result(r6, "128 ray steps");

    // Performance summary
    printf("Performance Summary:\n");
    printf("────────────────────────────────────────────────────────────────────────────────\n");
    printf("Resolution Impact:\n");
    printf("  640x480  → 1024x768:  %.1f%% FPS change\n",
           ((r2.avg_fps - r1.avg_fps) / r1.avg_fps) * 100);
    printf("  1024x768 → 1920x1080: %.1f%% FPS change\n",
           ((r3.avg_fps - r2.avg_fps) / r2.avg_fps) * 100);
    printf("\n");
    printf("Ray Step Impact (1024x768):\n");
    printf("  32  → 64  steps: %.1f%% FPS change\n",
           ((r5.avg_fps - r4.avg_fps) / r4.avg_fps) * 100);
    printf("  64  → 128 steps: %.1f%% FPS change\n",
           ((r6.avg_fps - r5.avg_fps) / r5.avg_fps) * 100);
    printf("\n");
    printf("Optimal Configuration:\n");
    printf("  For 60+ FPS: ");
    if (r2.avg_fps >= 60) printf("1024x768 @ 64 steps ✓\n");
    else if (r1.avg_fps >= 60) printf("640x480 @ 64 steps\n");
    else printf("Reduce resolution or ray steps\n");
    printf("\n");

    printf("================================================================================\n");
    printf("BENCHMARK COMPLETE\n");
    printf("================================================================================\n\n");

    return 0;
}
