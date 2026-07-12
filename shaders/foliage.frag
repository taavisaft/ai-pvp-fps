#version 330 core
in vec3 worldPos;
in vec3 vNormal;
in vec2 vUV;
in vec4 lightSpacePos;

uniform vec3 eyePos;
uniform vec3 sunDir;
uniform vec3 skyZenith;
uniform vec3 skyHorizon;
uniform vec3 groundAmbient;

// Atmosphere pass (preset-driven; mirrors basic.frag).
uniform vec3  sunColor;
uniform float fogDist;
uniform float fogHeightAmt;
uniform float cloudAmount;
uniform float exposure;
uniform float saturation;
uniform float time;

uniform sampler2D diffuseMap;   // baseColor (RGBA), texture unit 0
uniform float alphaCutoff;      // <0 = opaque; else discard alpha < cutoff
uniform sampler2D shadowMap;    // sun depth, texture unit 1
uniform int useShadow;

out vec4 fragColor;

// 3x3 PCF, matching basic.frag so trees sit in the same light as the ground.
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

// Cloud shadows + grade: identical math to basic.frag so trees darken with the
// ground under the same drifting cloud and land on the same filmic curve.
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
    vec4 tex = texture(diffuseMap, vUV);
    if (alphaCutoff >= 0.0 && tex.a < alphaCutoff) discard;   // leaf cutout
    // Small deterministic hue/value drift keeps large instanced stands from reading
    // as exact copies. It is derived from position, so no instance memory is added.
    float variation = fract(sin(dot(worldPos.xz, vec2(0.127, 0.311))) * 43758.5453);
    vec3 c = tex.rgb * mix(0.91, 1.07, variation);
    c *= mix(vec3(0.96, 1.02, 0.94), vec3(1.03, 0.98, 0.92), variation);

    vec3 n = normalize(vNormal);
    if (!gl_FrontFacing) n = -n;     // double-sided leaves: face the viewer
    vec3 L = normalize(sunDir);

    vec3 sun     = sunColor * max(dot(n, L), 0.0)
                 * sunVisibility(n, L) * cloudShadow(worldPos.xz, time);
    vec3 ambient = mix(groundAmbient, skyZenith, n.y * 0.5 + 0.5);
    vec3 lit     = c * (sun + ambient);

    float dens = 1.0 + fogHeightAmt * exp(-max(worldPos.y, 0.0) / 12.0);
    float fog  = clamp(length(worldPos - eyePos) * dens / fogDist, 0.0, 1.0);
    fragColor = vec4(grade(mix(lit, skyHorizon, fog * fog)), 1.0);
}
