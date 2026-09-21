#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 model,view,proj,captureVP;
out vec3 worldPos;
out vec4 reflectionPos;
void main() {
    worldPos=(model*vec4(aPos,1)).xyz;
    reflectionPos=captureVP*vec4(worldPos,1);
    gl_Position=proj*view*vec4(worldPos,1);
}
