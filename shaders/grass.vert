#version 330 core
layout(location=0) in vec3 aPos;layout(location=1) in vec3 aNormal;layout(location=3) in vec4 instancePosScale;
uniform mat4 view;uniform mat4 proj;uniform vec3 eyePos;uniform float time;
out vec3 worldPos;out vec3 vNormal;out float coverage;out float variation;
void main(){vec3 p=aPos*instancePosScale.w;float dist=length(instancePosScale.xz-eyePos.xz),fade=smoothstep(58.0,82.0,dist);p.y-=fade*.48;float tip=clamp(aPos.y/.75,0.0,1.0);p.x+=sin(time*1.7+instancePosScale.x*.12+instancePosScale.z*.09)*.085*tip;worldPos=instancePosScale.xyz+p;vNormal=aNormal;coverage=1.0-fade;variation=fract(sin(dot(instancePosScale.xz,vec2(12.9898,78.233)))*43758.5453);gl_Position=proj*view*vec4(worldPos,1.0);}
