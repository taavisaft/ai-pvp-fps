#version 330 core
// Sun-depth pass for instanced vegetation (tree LOD0 near the camera). Same
// instance layout and wind as veg.vert so cast shadows sway with the mesh.
layout(location = 0) in vec3  aPos;
layout(location = 3) in float aFlex;
layout(location = 4) in vec4  iA;
layout(location = 5) in vec4  iB;
layout(location = 6) in vec2  aUV;

out vec2 vUV;

uniform mat4  lightSpace;
uniform float time;
uniform float windAmp;

void main() {
    float c = cos(iB.x), s = sin(iB.x);
    vec3 p = vec3(c * aPos.x - s * aPos.z, aPos.y, s * aPos.x + c * aPos.z) * iA.w;
    float w = sin(time * 1.9 + iB.y * 6.2831 + dot(iA.xz, vec2(0.13, 0.09)))
            + 0.5 * sin(time * 3.7 + iB.y * 9.0);
    vec3 wp = iA.xyz + p;
    wp.xz += w * windAmp * aFlex * iA.w;
    vUV = aUV;
    gl_Position = lightSpace * vec4(wp, 1.0);
}
