#version 330 core
in vec2 vUV;in vec2 vLMUV;in vec3 vNorm;in vec4 vCol;
out vec4 FragColor;
uniform sampler2D uTex;uniform sampler2D uLM;uniform int uHasTex;
void main(){
vec4 c=uHasTex>0?texture(uTex,vUV):vec4(0.7);
FragColor=c;
}
