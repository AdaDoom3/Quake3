#version 330 core
layout(location=0)in vec3 aPos;
layout(location=1)in vec2 aUV;
layout(location=2)in vec2 aLMUV;
layout(location=3)in vec3 aNorm;
layout(location=4)in vec4 aCol;
out vec2 vUV;out vec2 vLMUV;out vec3 vNorm;out vec4 vCol;
uniform mat4 uProj,uView;
void main(){
vUV=aUV;vLMUV=aLMUV;vNorm=aNorm;vCol=aCol;
gl_Position=uProj*uView*vec4(aPos,1.0);
}
