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
    if ((aUV.x < -4.5 && aUV.x > -5.5 && iB.y < .97) || (aUV.x < -3.5 && aUV.x > -4.5 && iB.y < .72)) p=vec3(0);
    if (grassRange > 0.0 && aUV.x >= -2.5) {
        float dist=length(iA.xyz-grassEye);
        p.y *= 1.0-smoothstep(grassRange*.68,grassRange*.97,dist+fract(iB.y)*10.0);
        p.xz *= 1.0+dist*.006;
    }
    // Retain full-height clumps across the lobby, thinning by stable rank.
    // Must match meadow_density.h; shadows use the same camera and threshold.
    float densityGrow=1.0;
    if (aUV.x < -2.5 && grassRange > 0.0) {
        float dist=length(iA.xz-grassEye.xz);
        float d=max(0.0,dist-6.0)/8.0;
        float density=max(.004,1.18/(1.0+d*d));
        if(grassRange > 50.0) density*=1.0-smoothstep(40.0,50.0,dist);
        float rank=fract(iB.y*13.37);
        densityGrow=density>0.0 ? 1.0-smoothstep(density*.85,density,rank) : 0.0;
        // Narrow only the departing clumps; survivors retain their height.
        float lod=smoothstep(18.0+iB.y*6.0,26.0+iB.y*6.0,dist);
        bool farMesh=(aUV.x < -5.5 && aUV.x > -7.5) || aUV.x < -9.5;
        float coverage=mix(1.0,min(1.5,sqrt(1.0/max(density,.0001))),lod);
        float presence=farMesh ? lod : 1.0-lod;
        p.xz*=densityGrow*coverage*presence;
        if (densityGrow*presence <= 0.0) p=vec3(0);
        densityGrow*=presence*(1.0-lod);
    }
    float w = 0.0;
    if (aFlex > 0.0 && densityGrow > 0.0) w = sin(time * 1.9 + iB.y * 6.2831 + dot(iA.xz, vec2(0.13, 0.09)))
            + 0.5 * sin(time * 3.7 + iB.y * 9.0);
    vec3 wp = iA.xyz + p;
    wp.xz += w * windAmp * aFlex * iA.w * densityGrow;
    vUV = aUV;
    if(aUV.x < -7.5) {
        float u=(aUV.x < -9.5 ? -10.0 : -8.0)-aUV.x;
        float variant=fract(iB.y*7.13);
        float column=variant < .65 ? 0.0 : variant < .95 ? 1.0 : 2.0;
        vUV=vec2(-8.0-(column+mix(.018,.982,u))/3.0,aUV.y);
    }
    gl_Position = lightSpace * vec4(wp, 1.0);
}
