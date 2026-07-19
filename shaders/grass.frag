#version 330 core
in vec3  worldPos;
in vec3  vNormal;
in vec2  vUV;
in float vRootT;
in float vTintSeed;
in float vBladeClass;
in vec4  lightSpacePos;

uniform vec3 eyePos;
uniform vec3 sunDir;
uniform vec3 skyZenith;
uniform vec3 skyHorizon;
uniform vec3 groundAmbient;
uniform sampler2D diffuseMap;   // blade atlas (4 columns), texture unit 0
uniform sampler2D shadowMap;    // sun depth, texture unit 1
uniform int useShadow;

// Atmosphere pass (preset-driven; mirrors basic.frag).
uniform vec3  sunColor;
uniform float fogDist;
uniform float fogHeightAmt;
uniform float cloudAmount;
uniform float exposure;
uniform float saturation;
uniform float time;

out vec4 fragColor;

// Ring where 3D clumps hand over to the flat fragment-grass ground texture.
const float FADE_START = 43.0;
const float FADE_END   = 51.0;

float sunVisibility(vec3 n, vec3 L) {
    if (useShadow == 0) return 1.0;
    vec3 p = lightSpacePos.xyz / lightSpacePos.w;
    p = p * 0.5 + 0.5;
    if (p.z > 1.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return 1.0;
    float bias = max(0.0025 * (1.0 - dot(n, L)), 0.0006);
    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
    float vis = 0.0;
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++) {
            float d = texture(shadowMap, p.xy + vec2(x, y) * texel).r;
            vis += (p.z - bias > d) ? 0.0 : 1.0;
        }
    return vis / 9.0;
}

float chash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}
float cnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(chash(i), chash(i + vec2(1, 0)), f.x),
               mix(chash(i + vec2(0, 1)), chash(i + vec2(1, 1)), f.x), f.y);
}
float cloudShadow(vec2 xz, float t) {
    if (cloudAmount <= 0.0) return 1.0;
    float cl = cnoise(xz * 0.010 + t * 0.010) * 0.65
             + cnoise(xz * 0.027 - t * 0.013) * 0.35;
    return 1.0 - cloudAmount * (1.0 - smoothstep(0.35, 0.72, cl));
}
vec3 grade(vec3 c) {
    c *= exposure;
    c = mix(vec3(dot(c, vec3(0.299, 0.587, 0.114))), c, saturation);
    return clamp((c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    float dist = length(worldPos - eyePos);

    // Screen-door fade at the ring edge: distant clumps dissolve pixel-by-pixel
    // into the ground grass texture instead of popping.
    float fade = smoothstep(FADE_START, FADE_END, dist);
    if (fade > 0.0) {
        float dither = chash(floor(gl_FragCoord.xy));
        if (dither < fade) discard;
    }

    vec4 tex = texture(diffuseMap, vUV);
    // Blade cutout (atlas alpha is binarized on load, so edges are crisp). Mip
    // minification still averages alpha down, so the threshold relaxes a little
    // with distance to keep far clumps from eroding away.
    float cut = mix(0.55, 0.22, smoothstep(10.0, 35.0, dist));
    if (tex.a < cut) discard;

    // Per-clump tint drift so the field isn't one flat green, and root blend: the
    // card base fades toward the ground albedo so clumps grow out of the soil
    // instead of sitting on it (vRootT: 0 = root baseline, 1 = blade tip).
    vec3 greenGrade = mix(vec3(0.86, 0.93, 0.72),
                          vec3(1.00, 0.86, 0.65), vTintSeed);
    vec3 c = tex.rgb * greenGrade * (0.92 + 0.18 * vTintSeed);
    float dryPatch = smoothstep(0.84, 0.99, vTintSeed);
    c = mix(c, vec3(0.46, 0.40, 0.19), dryPatch * 0.30);
    if (vBladeClass > 1.5) c = mix(c, vec3(0.48, 0.41, 0.20), 0.38);
    // Dark root occlusion makes overlapping tufts read as a thick under-storey.
    c *= mix(0.76, 1.0, smoothstep(0.02, 0.42, vRootT));
    c = mix(c, vec3(0.36, 0.34, 0.20), (1.0 - smoothstep(0.0, 0.13, vRootT)) * 0.30);

    // Lit with the TERRAIN normal, not the quad normal — every blade shades like
    // the ground beneath it, so clumps melt into the field instead of sparkling.
    vec3 n = normalize(vNormal);
    vec3 L = normalize(sunDir);
    vec3 sun     = sunColor * max(dot(n, L), 0.0)
                 * sunVisibility(n, L) * cloudShadow(worldPos.xz, time);
    vec3 ambient = mix(groundAmbient, skyZenith, n.y * 0.5 + 0.5);
    vec3 lit     = c * (sun + ambient);

    float dens = 1.0 + fogHeightAmt * exp(-max(worldPos.y, 0.0) / 12.0);
    float fog  = clamp(dist * dens / fogDist, 0.0, 1.0);
    fragColor = vec4(grade(mix(lit, skyHorizon, fog * fog)), 1.0);
}
