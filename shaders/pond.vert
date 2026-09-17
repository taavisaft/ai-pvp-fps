#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 model,view,proj;
out vec3 worldPos;
out vec4 clipPos;
void main() {
    worldPos=(model*vec4(aPos,1)).xyz;
    clipPos=proj*view*vec4(worldPos,1);
    gl_Position=clipPos;
}
