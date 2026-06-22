#version 330 core
layout(location = 0) in vec3 aPos;   // unit quad, -0.5..0.5 in XY (HUD space)
uniform mat4 model;                   // NDC placement (no view/proj for HUD)
uniform vec2 uvCenter;                // sampled sub-rect center in [0,1]
uniform vec2 uvHalf;                  // sub-rect half-size (0.5 = whole image)
out vec2 vUV;
void main() {
    vec2 q = aPos.xy + 0.5;                       // 0..1 across the quad
    vUV = uvCenter + (q - 0.5) * 2.0 * uvHalf;     // map quad -> sub-rect of the image
    gl_Position = model * vec4(aPos, 1.0);
}
