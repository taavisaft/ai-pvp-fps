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
uniform vec3 grassEye;
uniform float grassRange;

void main() {
    float c = cos(iB.x), s = sin(iB.x);
    vec3 p = vec3(c * aPos.x - s * aPos.z, aPos.y, s * aPos.x + c * aPos.z) * iA.w;
    // Different plants share a batch; uncommon leaves/seed heads collapse away.
    if ((aUV.x < -4.5 && iB.y < .97) || (aUV.x < -3.5 && aUV.x > -4.5 && iB.y < .72)) p=vec3(0);
    if (grassRange > 0.0) {
        float dist=length(iA.xyz-grassEye);
        p.y *= 1.0-smoothstep(grassRange*.68,grassRange*.97,dist+fract(iB.y)*10.0);
        p.xz *= 1.0+dist*.006;
    }
    // Stable per-plant density: full within 6 m, about 22% at 28 m.
    // Must match meadow_density.h; shadows use the same camera and threshold.
    float densityGrow=1.0;
    if (aUV.x < -2.5 && grassRange > 0.0) {
        float density=mix(1.06,.22,smoothstep(6.0,28.0,length(iA.xz-grassEye.xz)));
        float rank=fract(iB.y*13.37);
        densityGrow=1.0-smoothstep(density-.06,density,rank);
        p*=densityGrow;
    }
    float w = sin(time * 1.9 + iB.y * 6.2831 + dot(iA.xz, vec2(0.13, 0.09)))
            + 0.5 * sin(time * 3.7 + iB.y * 9.0);
    vec3 wp = iA.xyz + p;
    wp.xz += w * windAmp * aFlex * iA.w * densityGrow;
    vUV = aUV;
    gl_Position = lightSpace * vec4(wp, 1.0);
}
