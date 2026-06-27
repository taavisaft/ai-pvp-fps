#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;     // unused (depth only)
layout(location = 2) in vec3 aColor;      // unused (depth only)
layout(location = 3) in vec3 iPos;
layout(location = 4) in vec2 iYawScale;

uniform mat4 lightSpace;

void main() {
    float yaw = iYawScale.x, scale = iYawScale.y;
    float c = cos(yaw), s = sin(yaw);
    mat3 rot = mat3(c, 0.0, -s,   0.0, 1.0, 0.0,   s, 0.0, c);
    vec3 wp = iPos + rot * (aPos * scale);
    gl_Position = lightSpace * vec4(wp, 1.0);
}
