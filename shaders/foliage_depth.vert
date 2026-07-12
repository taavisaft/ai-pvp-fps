#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;     // unused (depth only)
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 iPos;
layout(location = 4) in vec2 iYawScale;
layout(location = 5) in float iTile;

uniform mat4 lightSpace;
uniform float time;

out vec2 vUV;

void main() {
    float yaw = iYawScale.x, scale = iYawScale.y;
    float c = cos(yaw), s = sin(yaw);
    mat3 rot = mat3(c, 0.0, -s,   0.0, 1.0, 0.0,   s, 0.0, c);

    float shape = fract(sin(dot(iPos.xz, vec2(12.9898, 78.233))) * 43758.5453);
    vec3 lp = aPos * scale;
    lp.xz *= mix(0.82, 1.16, shape);
    // Match foliage.vert sway exactly so the shadow tracks the swaying canopy.
    float h = clamp(lp.y / (8.0 * scale), 0.0, 1.0);
    lp.x += sin(time * 1.3 + iPos.x * 0.5 + iPos.z * 0.5) * 0.25 * scale * h * h;
    lp.z += cos(time * 1.1 + iPos.x * 0.4 - iPos.z * 0.3) * 0.18 * scale * h * h;

    vec3 wp = iPos + rot * lp;
    float col = mod(iTile, 4.0), row = floor(iTile / 4.0);
    vUV = (vec2(col, 3.0 - row) + aUV) * 0.25;
    gl_Position = lightSpace * vec4(wp, 1.0);
}
