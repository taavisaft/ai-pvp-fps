#version 330 core
// Fullscreen pass. Reuses the unit HUD quad (aPos in [-0.5,0.5]); expands to NDC
// and hands the clip-space XY to the fragment shader for view-ray reconstruction.
layout(location = 0) in vec3 aPos;
out vec2 vNdc;
void main() {
    vNdc = aPos.xy * 2.0;
    gl_Position = vec4(aPos.xy * 2.0, 0.0, 1.0);
}
