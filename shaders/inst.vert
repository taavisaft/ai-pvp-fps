#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec3 iPos;        // per-instance world position (base, y=ground)
layout(location = 4) in vec2 iYawScale;   // per-instance yaw (rad), uniform scale

uniform mat4 view;
uniform mat4 proj;
uniform mat4 lightSpace;

out vec3 worldPos;
out vec3 vNormal;
out vec3 vColor;
out vec4 lightSpacePos;

void main() {
    float yaw = iYawScale.x, scale = iYawScale.y;
    float c = cos(yaw), s = sin(yaw);
    mat3 rot = mat3(c, 0.0, -s,   0.0, 1.0, 0.0,   s, 0.0, c);

    vec3 wp = iPos + rot * (aPos * scale);
    worldPos      = wp;
    vNormal       = rot * aNormal;
    vColor        = aColor;
    lightSpacePos = lightSpace * vec4(wp, 1.0);
    gl_Position   = proj * view * vec4(wp, 1.0);
}
