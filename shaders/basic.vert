#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;   // supplied only by meshes with hasNormal=1
layout(location = 2) in vec2 aUV;       // supplied only by UV meshes (building facades)
uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
uniform mat4 lightSpace;   // sun's view-proj, for shadow lookup
out vec3 worldPos;
out vec3 vNormal;
out vec2 vUV;
out vec4 lightSpacePos;
void main() {
    vec4 wp = model * vec4(aPos, 1.0);
    worldPos = wp.xyz;
    vNormal = mat3(model) * aNormal;
    vUV = aUV;
    lightSpacePos = lightSpace * wp;
    gl_Position = proj * view * wp;
}
