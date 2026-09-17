#version 330 core
// Impostor shading: albedo from the bake atlas, lit by a flat approximation of
// the daylight terms (impostors start past 280 m — per-pixel normals would be
// invisible there), then the same cloud/fog/grade chain as every world shader.
in vec3  worldPos;
in vec2  vUV;
in float vTint;
in float vCutNear;
in float vSide;
in float vShade;
in float vHue;

uniform sampler2D impTex;
uniform vec3  eyePos;
uniform float time;

uniform vec3  sunDir;
uniform vec3  sunColor;
uniform vec3  skyZenith;
uniform vec3  skyHorizon;
uniform vec3  groundAmbient;
uniform float fogDist;
uniform float fogHeightAmt;
uniform float cloudAmount;
uniform float exposure;
uniform float saturation;

out vec4 fragColor;
#include "atmosphere.glsl"

float bayer(vec2 p) {
    int x = int(mod(p.x, 4.0));
    int y = int(mod(p.y, 4.0));
    int m[16] = int[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
    return (float(m[y * 4 + x]) + 0.5) / 16.0;
}

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
float cloudShadow(vec2 xz, float t) {
    if (cloudAmount <= 0.0) return 1.0;
    float cl = vnoise(xz * 0.010 + t * 0.010) * 0.65
             + vnoise(xz * 0.027 - t * 0.013) * 0.35;
    return 1.0 - cloudAmount * (1.0 - smoothstep(0.35, 0.72, cl));
}

vec3 grade(vec3 c) {
    c *= exposure;
    c = mix(vec3(dot(c, vec3(0.299, 0.587, 0.114))), c, saturation);
    return clamp((c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14), 0.0, 1.0);
}

uniform float clipWater;
uniform int alphaToCoverage;
void main() {
    if(clipWater>0.0 && worldPos.y<clipWater) discard;
    if (bayer(gl_FragCoord.xy) < vCutNear) discard;
    vec4 texel = texture(impTex, vUV);
    float coverage = clamp((texel.a - 0.3) / max(fwidth(texel.a), 0.0001) + 0.5, 0.0, 1.0);
    if (coverage < (alphaToCoverage == 1 ? 0.004 : 0.5)) discard;

    // Flat light: averaged sun + hemispheric ambient, tuned to sit level with the
    // mesh LODs' per-normal lighting so the dither band doesn't shift brightness.
    float side = clamp(0.5 + (vUV.x - 0.5) * 1.6 * vSide, 0.0, 1.0);
    float crownLight = mix(0.22, 0.95, side) * mix(0.70, 1.10, vUV.y);
    float neighbours = 1.0 - 0.8 * vShade * (1.0 - 0.75 * vUV.y);
    vec3 hue = mix(vec3(0.94, 1.0, 0.92), vec3(1.06, 1.0, 0.90), vHue);
    vec3 lit3 = texel.rgb * vTint * hue
              * (sunColor * crownLight * neighbours * cloudShadow(worldPos.xz, time)
                 + mix(groundAmbient, skyZenith, 0.72) * mix(0.72, 1.0, vUV.y));

    float dens = 1.0 + fogHeightAmt * exp(-max(worldPos.y, 0.0) / 12.0);
    float fog  = clamp(length(worldPos - eyePos) * dens / fogDist, 0.0, 1.0);
    fragColor = vec4(grade(mix(lit3, fogColor(worldPos - eyePos), fog * fog)), coverage);
}
