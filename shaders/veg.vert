#version 330 core
#include "training_tree.glsl"
// Instanced vegetation (grass blades + spruce LOD meshes). Per-vertex: position,
// normal, color, flex (0 root .. 1 tip, scales wind sway). Per-instance: two vec4s
// A=(world x,y,z, uniform scale) B=(yaw, wind phase, brightness, dry factor).
layout(location = 0) in vec3  aPos;
layout(location = 1) in vec3  aNormal;
layout(location = 2) in vec3  aColor;
layout(location = 3) in float aFlex;
layout(location = 4) in vec4  iA;
layout(location = 5) in vec4  iB;
layout(location = 6) in vec2  aUV;   // (-1,-1) = untextured vertex

uniform mat4  view;
uniform mat4  proj;
uniform mat4  lightSpace;
uniform vec3  eyePos;
uniform float time;
uniform float windAmp;    // meters of sway at flex=1, scale=1
uniform float grassRange; // >0 = grass mode: blades sink to 0 near this distance
// LOD cross-fade bands (start,end in meters). Fragment keeps pixels where
// cutNear <= bayer < cutFar, so consecutive LODs drawn with mirrored bands are
// exact complements — every pixel drawn exactly once, no overlap and no gap.
uniform vec2  fadeIn;     // this LOD dithers IN across this band (0,0 = always on)
uniform vec2  fadeOut;    // this LOD dithers OUT across this band (0,0 = never)

out vec3  worldPos;
out vec3 treeLocal;
out vec3 treeUnit;
out vec3 terrainNormal;
flat out float trainingMeadow;
out vec3  vNormal;
out vec3  vColor;
out vec2  vUV;
out vec4  lightSpacePos;
out float vCutNear;
out float vCutFar;

void main() {
    trainingMeadow=(grassRange>0.0 && grassRange<50.0 && aUV.x < -7.5) ? 1.0 : 0.0;
    float c = cos(iB.x), s = sin(iB.x);
    vec3 shaped=trainingTreeShape(aPos,aUV,iA);
    treeLocal=aPos*iA.w;
    treeUnit=aPos;
    vec3 p = vec3(c * shaped.x - s * shaped.z, shaped.y, s * shaped.x + c * shaped.z);
    vec3 n = vec3(c * aNormal.x - s * aNormal.z, aNormal.y, s * aNormal.x + c * aNormal.z);

    float dist = length(iA.xyz - eyePos);
    float grow = 1.0;
    if (grassRange > 0.0 && aUV.x >= -2.5) {
        // Sink blades into the ground approaching the range edge (per-blade jitter
        // staggers the sink) — the terrain's procedural grass color carries on from
        // there, so the transition line never reads as an edge.
        grow = 1.0 - smoothstep(grassRange * 0.68, grassRange * 0.97,
                                dist + fract(iB.y) * 10.0);
        // Widen far blades: keeps sub-pixel straws from dissolving into shimmer.
        p.xz *= 1.0 + dist * 0.006;
    }
    // Seed heads and broadleaf rosettes occur in a minority of training tufts.
    if(trainingMeadow>0.5 && ((aColor.z==3.0 && iB.y<.88) ||
                              (aColor.z==4.0 && iB.y<.94))) p=vec3(0);
    p *= iA.w * vec3(1.0, grow, 1.0);

    // Wind: two sines with per-instance phase plus a spatial term so gusts travel
    // across the field as waves instead of the whole map rocking in unison.
    // Different plants share a batch; uncommon leaves/seed heads collapse away.
    if ((aUV.x < -4.5 && aUV.x > -5.5 && iB.y < .97) || (aUV.x < -3.5 && aUV.x > -4.5 && iB.y < .72)) p=vec3(0);
    // Retain full-height clumps across the lobby, thinning by stable rank.
    // Must match meadow_density.h; shadows use the same camera and threshold.
    float densityGrow=1.0;
    if (aUV.x < -2.5 && grassRange > 0.0) {
        float dist=length(iA.xz-eyePos.xz);
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

    worldPos = wp;
    vNormal  = n;
    terrainNormal=n;
    if(aUV.x < -7.5) {
        vec2 xz=vec2(mod(iB.w,256.0),floor(iB.w/256.0))/255.0*2.0-1.0;
        terrainNormal=normalize(vec3(xz.x,sqrt(max(.01,1.0-dot(xz,xz))),xz.y));
    }
    // Dry-grass tint (iB.w) matches the ground shader's dry patches; iB.z is a
    // per-instance brightness jitter that breaks up the uniform green.
    vColor   = mix(aColor, vec3(0.33, 0.30, 0.14), iB.w) * iB.z;
    if (aUV.x < -2.5 && aUV.x > -7.5 && grassRange > 0.0) {
        // At distance the ground supplies coverage; suppress isolated dark roots,
        // dry clumps and brightness variation instead of amplifying their spots.
        float unified=smoothstep(16.0,44.0,length(iA.xz-eyePos.xz));
        vColor=mix(vColor,vec3(.145,.19,.05),unified*.85);
    }
    if(aUV.x < -7.5) {
        float dry=smoothstep(.84,.99,fract(iB.y*19.13+aColor.x));
        vec3 blade=mix(vec3(.10,.125,.045),vec3(.30,.355,.145),aUV.y);
        blade=mix(blade,vec3(.27,.225,.125),dry*.65);
        if(trainingMeadow>0.5) {
            float straw=aColor.z==2.0 || aColor.z==3.0 ? 1.0 : 0.0;
            blade=mix(vec3(.13,.18,.06),vec3(.32,.39,.17),aUV.y);
            blade=mix(blade,mix(vec3(.24,.21,.115),vec3(.49,.44,.285),aUV.y),straw);
            if(aColor.z==4.0) blade*=vec3(.75,.94,.75);
        }
        vColor=blade*(.82+.3*aColor.y)*iB.z;
    }
    lightSpacePos = lightSpace * vec4(wp, 1.0);
    vUV = aUV;
    if(aUV.x < -7.5) {
        float u=(aUV.x < -9.5 ? -10.0 : -8.0)-aUV.x;
        float variant=fract(iB.y*7.13);
        float column=variant < .65 ? 0.0 : variant < .95 ? 1.0 : 2.0;
        vUV=vec2(-8.0-(column+mix(.018,.982,u))/3.0,aUV.y);
    }

    vCutNear = (fadeIn.y  > fadeIn.x)  ? 1.0 - smoothstep(fadeIn.x,  fadeIn.y,  dist) : 0.0;
    vCutFar  = (fadeOut.y > fadeOut.x) ? 1.0 - smoothstep(fadeOut.x, fadeOut.y, dist) : 1.0;

    gl_Position = proj * view * vec4(wp, 1.0);
}
