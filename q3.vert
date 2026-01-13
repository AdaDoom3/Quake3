#version 450
layout(location=0)in vec3 p;
layout(location=1)in vec3 n;
layout(location=2)in vec2 u;
layout(location=3)in vec2 l;
layout(location=0)out vec3 fn;
layout(location=1)out vec2 fu;
layout(location=2)out vec2 fl;
layout(location=3)out vec3 fp;
layout(binding=0)uniform U{mat4 vp;mat4 m;}ub;
void main(){gl_Position=ub.vp*vec4(p,1);fn=n;fu=u;fl=l;fp=p;}
