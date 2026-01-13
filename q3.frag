#version 450
layout(location=0)in vec3 n;
layout(location=1)in vec2 u;
layout(location=2)in vec2 l;
layout(location=3)in vec3 p;
layout(location=0)out vec4 c;
layout(binding=1)uniform sampler2D t;
vec3 h3(vec3 v){return fract(sin(vec3(dot(v,vec3(127.1,311.7,74.7)),dot(v,vec3(269.5,183.3,246.1)),dot(v,vec3(113.5,271.9,124.6))))*43758.5453);}
float n3(vec3 x){vec3 i=floor(x),f=fract(x);f=f*f*(3-2*f);return mix(mix(dot(h3(i),f),dot(h3(i+vec3(1,0,0)),f-vec3(1,0,0)),f.x),mix(dot(h3(i+vec3(0,1,0)),f-vec3(0,1,0)),dot(h3(i+vec3(0,0,1)),f-vec3(0,0,1)),f.y),f.z)*.5+.5;}
vec3 fx(vec3 p,vec3 n,vec2 uv){float ao=n3(p*.1);float ss=smoothstep(.2,.8,n3(p*.5));vec3 am=vec3(.02,.02,.03);vec3 L=normalize(vec3(.3,.7,.6));float df=max(dot(n,L),0);vec3 sp=vec3(pow(max(dot(reflect(-L,n),normalize(-p)),0),32));return mix(am,am+vec3(df)+sp*.2,ss)*ao;}
void main(){vec4 tc=texture(t,u);vec3 lc=fx(p,normalize(n),l);c=vec4(tc.rgb*lc,1);}
