#version 450
layout(location=0)in vec2 u0;
layout(location=1)in vec2 u1;
layout(location=2)in vec3 n;
layout(location=3)in vec4 c;
layout(location=4)in vec3 wp;
layout(location=0)out vec4 O;
layout(binding=1)uniform sampler2D T;
layout(binding=2)uniform sampler2D L;
layout(push_constant)uniform PC{uint m;}pc;
vec3 pal(float t){
return.5+.5*cos(6.28*(vec3(1,.7,.4)*t+vec3(0,.15,.2)));
}
float h21(vec2 p){
return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5);
}
vec3 tr(vec3 p,vec3 n){
vec3 l=normalize(vec3(.7,.3,1));
float d=max(dot(n,l),.2);
float a=max(0.,dot(reflect(-l,n),normalize(-p)));
float s=pow(a,32.)*.5;
return vec3(d+s);
}
void main(){
vec3 tc=c.rgb;
if(pc.m==0){
vec3 lc=vec3(1);
vec3 li=tr(wp,normalize(n));
O=vec4(tc*lc*li,1);
}else{
float r=h21(floor(u0*8.));
vec3 col=pal(r);
O=vec4(col*tr(wp,normalize(n)),1);
}
}
