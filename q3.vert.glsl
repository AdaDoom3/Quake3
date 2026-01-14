#version 450
layout(location=0)in vec3 P;
layout(location=1)in vec2 U0;
layout(location=2)in vec2 U1;
layout(location=3)in vec3 N;
layout(location=4)in vec4 C;
layout(binding=0)uniform U{mat4 V;mat4 P;}u;
layout(location=0)out vec2 u0;
layout(location=1)out vec2 u1;
layout(location=2)out vec3 n;
layout(location=3)out vec4 c;
layout(location=4)out vec3 wp;
void main(){
vec4 p=u.P*u.V*vec4(P,1);
gl_Position=p;
u0=U0;u1=U1;n=N;c=C;wp=P;
}
