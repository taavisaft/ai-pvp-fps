#version 330 core
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
out vec3  vNormal;
out vec3  vColor;
out vec2  vUV;
out vec4  lightSpacePos;
out float vCutNear;
out float vCutFar;

void main() {
    float c = cos(iB.x), s = sin(iB.x);
    vec3 p = vec3(c * aPos.x - s * aPos.z, aPos.y, s * aPos.x + c * aPos.z);
    vec3 n = vec3(c * aNormal.x - s * aNormal.z, aNormal.y, s * aNormal.x + c * aNormal.z);

    float dist = length(iA.xyz - eyePos);
    float grow = 1.0;
    if (grassRange > 0.0) {
        // Sink blades into the ground approaching the range edge (per-blade jitter
        // staggers the sink) — the terrain's procedural grass color carries on from
        // there, so the transition line never reads as an edge.
        grow = 1.0 - smoothstep(grassRange * 0.68, grassRange * 0.97,
                                dist + fract(iB.y) * 10.0);
        // Widen far blades: keeps sub-pixel straws from dissolving into shimmer.
        p.xz *= 1.0 + dist * 0.006;
    }
    p *= iA.w * vec3(1.0, grow, 1.0);

    // Wind: two sines with per-instance phase plus a spatial term so gusts travel
    // across the field as waves instead of the whole map rocking in unison.
    float w = sin(time * 1.9 + iB.y * 6.2831 + dot(iA.xz, vec2(0.13, 0.09)))
            + 0.5 * sin(time * 3.7 + iB.y * 9.0);
    vec3 wp = iA.xyz + p;
    wp.xz += w * windAmp * aFlex * iA.w;

    worldPos = wp;
    vNormal  = n;
    // Dry-grass tint (iB.w) matches the ground shader's dry patches; iB.z is a
    // per-instance brightness jitter that breaks up the uniform green.
    vColor   = mix(aColor, vec3(0.33, 0.30, 0.14), iB.w) * iB.z;
    lightSpacePos = lightSpace * vec4(wp, 1.0);
    vUV = aUV;

    vCutNear = (fadeIn.y  > fadeIn.x)  ? 1.0 - smoothstep(fadeIn.x,  fadeIn.y,  dist) : 0.0;
    vCutFar  = (fadeOut.y > fadeOut.x) ? 1.0 - smoothstep(fadeOut.x, fadeOut.y, dist) : 1.0;

    gl_Position = proj * view * vec4(wp, 1.0);
}
