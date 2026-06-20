#version 330 core
layout(location = 0) in vec2 aCorner;     // quad corner, -1..1 in XZ
layout(location = 3) in vec3 iPos;        // per-instance tree position (sunk base)
layout(location = 4) in vec2 iYawScale;   // per-instance yaw, uniform scale

uniform mat4 view;
uniform mat4 proj;
uniform float radius;        // blob radius at scale 1 (canopy footprint)
uniform float groundOffset;  // lift from the sunk base up onto the ground surface

out vec2 vCorner;
out vec3 worldPos;

void main() {
    float scale = iYawScale.y;
    float r = radius * scale;
    vec3 wp = vec3(iPos.x + aCorner.x * r, iPos.y + groundOffset, iPos.z + aCorner.y * r);
    vCorner  = aCorner;
    worldPos = wp;
    gl_Position = proj * view * vec4(wp, 1.0);
}
