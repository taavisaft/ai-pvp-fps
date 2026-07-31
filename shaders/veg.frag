#version 330 core
// Vegetation lighting: same 3-term daylight + shadow + fog + grade as basic.frag,
// minus the splat/triplanar machinery. Adds the screen-door LOD cross-fade and
// two-sided normals (blades and cone skirts are drawn without face culling).
in vec3  worldPos;
in vec3  vNormal;
in vec3  vColor;
in vec2  vUV;
in vec4  lightSpacePos;
in float vCutNear;
in float vCutFar;

uniform vec3  eyePos;
uniform float time;
uniform int   bake;         // 1 = impostor bake: output raw albedo, no light/fog

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

uniform sampler2D shadowMap;
uniform sampler2D branchTex;   // needle-spray photo, alpha cutout
uniform int       useShadow;

out vec4 fragColor;

float bayer(vec2 p) {
    // 4x4 ordered-dither threshold, stable per screen pixel: complementary LOD
    // draws split pixels instead of blending, so no sorting and no double-cover.
    int x = int(mod(p.x, 4.0));
    int y = int(mod(p.y, 4.0));
    int m[16] = int[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
    return (float(m[y * 4 + x]) + 0.5) / 16.0;
}

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

void main() {
    // Branch cards: photo albedo, cut out by alpha. Trunk/blades (uv sentinel
    // -1) shade from the vertex color alone. vColor is the card's shade jitter.
    vec3 albedo = vColor;
    if (vUV.x >= 0.0) {
        vec4 t = texture(branchTex, vUV);
        if (t.a < 0.42) discard;
        albedo *= t.rgb;
    }

    if (bake == 1) {   // impostor capture: raw albedo, lit later by the billboard
        fragColor = vec4(albedo, 1.0);
        return;
    }

    float th = bayer(gl_FragCoord.xy);
    if (th < vCutNear || th >= vCutFar) discard;

    vec3 V = normalize(eyePos - worldPos);
    vec3 n = normalize(vNormal);
    if (dot(n, V) < 0.0) n = -n;   // two-sided: culling is off for vegetation
    vec3 L = normalize(sunDir);

    vec3 sun     = sunColor * max(dot(n, L), 0.0)
                 * sunVisibility(n, L) * cloudShadow(worldPos.xz, time);
    vec3 ambient = mix(groundAmbient, skyZenith, n.y * 0.5 + 0.5);
    vec3 lit3    = albedo * (sun + ambient);

    float dens = 1.0 + fogHeightAmt * exp(-max(worldPos.y, 0.0) / 12.0);
    float fog  = clamp(length(worldPos - eyePos) * dens / fogDist, 0.0, 1.0);
    fragColor = vec4(grade(mix(lit3, skyHorizon, fog * fog)), 1.0);
}
