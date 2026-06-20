#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 iPos;        // per-instance world position (feet)
layout(location = 4) in vec2 iYawScale;   // per-instance yaw (rad), uniform scale

uniform mat4 view;
uniform mat4 proj;
uniform mat4 lightSpace;
uniform float time;

out vec3 worldPos;
out vec3 vNormal;
out vec2 vUV;
out vec4 lightSpacePos;

void main() {
    float yaw = iYawScale.x, scale = iYawScale.y;
    float c = cos(yaw), s = sin(yaw);
    mat3 rot = mat3(c, 0.0, -s,   0.0, 1.0, 0.0,   s, 0.0, c);

    vec3 lp = aPos * scale;
    // Wind sway grows with height up the tree; phase varies per tree position.
    float h = clamp(lp.y / (8.0 * scale), 0.0, 1.0);
    lp.x += sin(time * 1.3 + iPos.x * 0.5 + iPos.z * 0.5) * 0.25 * scale * h * h;
    lp.z += cos(time * 1.1 + iPos.x * 0.4 - iPos.z * 0.3) * 0.18 * scale * h * h;

    vec3 wp = iPos + rot * lp;
    worldPos      = wp;
    vNormal       = rot * aNormal;
    vUV           = vec2(aUV.x, 1.0 - aUV.y);   // glTF top-left -> GL bottom-left
    lightSpacePos = lightSpace * vec4(wp, 1.0);
    gl_Position   = proj * view * vec4(wp, 1.0);
}
