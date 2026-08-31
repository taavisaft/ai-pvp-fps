#version 330 core
// Gradient sky + sun disk + procedural cloud layer. The view ray is rebuilt by
// unprojecting NDC (invViewProj) so the gradient stays anchored to world-up.
// Cloud noise uses the same field as basic.frag/veg.frag cloudShadow() so
// bright sky puffs line up with the shadows sweeping across the ground.
in vec2 vNdc;
uniform mat4 invViewProj;
uniform vec3 eyePos;
uniform vec3 sunDir;      // direction TOWARD the sun, normalized
uniform vec3 sunColor;
uniform vec3 skyZenith;
uniform vec3 skyHorizon;
uniform float time;
uniform float cloudAmount;  // 0..1, same as ground cloud-shadow strength
uniform float exposure;
uniform float saturation;
out vec4 fragColor;

vec3 grade(vec3 c) {
    c *= exposure;
    c = mix(vec3(dot(c, vec3(0.299, 0.587, 0.114))), c, saturation);
    return clamp((c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14), 0.0, 1.0);
}

// ---- cloud field (keep in sync with basic.frag cloudShadow) -------------------
float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x),
               mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
// Raw 0..1 density before smoothstep — same weights/speeds as the ground pass.
float cloudField(vec2 xz, float t) {
    return vnoise(xz * 0.010 + t * 0.010) * 0.65
         + vnoise(xz * 0.027 - t * 0.013) * 0.35;
}

// Map a view ray to the world XZ column whose shadow that cloud would cast below.
vec2 cloudSampleXZ(vec3 eye, vec3 dir) {
    if (dir.y < -0.002) {
        // Looking toward the landscape: hit the ground plane — matches worldPos.xz
        // on terrain fragments for the same view direction.
        float tg = (0.0 - eye.y) / dir.y;
        return eye.xz + dir.xz * tg;
    }
    if (dir.y > 0.015) {
        // Overhead sky: virtual slab so zenith still carries drifting structure.
        float tc = (900.0 - eye.y) / dir.y;
        return eye.xz + dir.xz * tc;
    }
    // Horizon band: long horizontal reach so the seam stays continuous.
    return eye.xz + dir.xz * 5000.0;
}

void main() {
    vec4 nearP = invViewProj * vec4(vNdc, -1.0, 1.0);
    vec4 farP  = invViewProj * vec4(vNdc,  1.0, 1.0);
    vec3 dir   = normalize(farP.xyz / farP.w - nearP.xyz / nearP.w);

    float sunElev = clamp(normalize(sunDir).y, 0.0, 1.0);
    float up      = clamp(dir.y, 0.0, 1.0);

    // Sun elevation tints the horizon (golden hour / overcast read).
    vec3 horizonCol = mix(skyHorizon, sunColor * 0.35 + skyHorizon * 0.65,
                          (1.0 - sunElev) * 0.55);
    vec3 col = mix(horizonCol, skyZenith, pow(up, mix(0.38, 0.52, sunElev)));

    // Procedural cloud layer — visible puffs synced with ground cloud shadows.
    if (cloudAmount > 0.001) {
        vec2 xz   = cloudSampleXZ(eyePos, dir);
        float cl  = cloudField(xz, time);
        float dens = smoothstep(0.35, 0.72, cl);
        // Wispy high-frequency edge breakup (cheap second sample, same drift).
        float wisps = vnoise(xz * 0.055 + time * 0.008);
        dens *= 0.82 + 0.18 * smoothstep(0.25, 0.75, wisps);
        // Fade clouds near the horizon line so they don't stack on the fog seam.
        float skyGate = smoothstep(-0.02, 0.12, dir.y) * smoothstep(1.0, 0.55, up);
        float cover = cloudAmount * dens * skyGate;
        vec3 cloudLit = mix(vec3(0.92, 0.94, 0.98), sunColor * 0.55 + vec3(0.45),
                            clamp(dot(normalize(sunDir), vec3(0, 1, 0)) * 0.5 + 0.5, 0.0, 1.0));
        // Thicker clouds pick up a cooler grey base (overcast preset).
        cloudLit = mix(cloudLit, vec3(0.78, 0.80, 0.84), dens * (1.0 - sunElev) * 0.5);
        col = mix(col, cloudLit, cover * 0.88);
    }

    // Sun disk: lower elevation = softer, larger glow (golden hour / overcast).
    vec3  L   = normalize(sunDir);
    float s   = max(dot(dir, L), 0.0);
    float lum = clamp(dot(sunColor, vec3(0.4)), 0.0, 1.5);
    float corePow = mix(450.0, 1100.0, sunElev);
    float haloPow = mix(10.0, 28.0, sunElev);
    col += sunColor * pow(s, corePow) * mix(1.4, 0.9, sunElev) * lum;
    col += sunColor * pow(s, haloPow)  * mix(0.35, 0.18, sunElev) * lum;
    // Low-angle atmospheric bloom around the sun.
    col += sunColor * pow(s, 6.0) * (1.0 - sunElev) * 0.12 * lum;

    fragColor = vec4(grade(col), 1.0);
}
