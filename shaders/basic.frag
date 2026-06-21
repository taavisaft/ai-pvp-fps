#version 330 core
in vec3 worldPos;
in vec3 vNormal;
in vec4 lightSpacePos;
uniform vec3 color;
uniform int  hasNormal;   // 1 = use supplied vertex normal (terrain), 0 = derivative
uniform float alpha;
uniform int lit;        // 1 = world pass (lighting/grid/fog), 0 = HUD
uniform vec3 eyePos;

uniform sampler2D diffuseMap;
uniform int useTexture;   // 0 = flat color, 1 = triplanar sample
uniform float tileSize;   // world meters per repeat
uniform float specular;     // view-aligned highlight strength
uniform vec3 tint;

uniform float time;       // seconds, for wind shimmer
uniform int   grass;      // 1 = procedural grass (ground only)

// Shared daylight palette (same values fed to the sky pass — see Renderer).
uniform vec3 sunDir;       // direction TOWARD the sun, normalized
uniform vec3 skyZenith;    // hemispheric ambient from above
uniform vec3 skyHorizon;   // horizon / distance-fog color
uniform vec3 groundAmbient;// ambient + bounce from below

uniform sampler2D shadowMap;  // sun depth, texture unit 1
uniform int useShadow;        // 1 = sample shadow map, 0 = fully lit

// Terrain slope/height splat (terrain draw only).
uniform int       splat;      // 1 = blend grass/dirt/rock by slope+height
uniform sampler2D rockMap;    // texture unit 2
uniform sampler2D dirtMap;    // texture unit 3
uniform float     rockTile;   // world meters per rock repeat
uniform float     dirtTile;   // world meters per dirt repeat

out vec4 fragColor;

// Fraction of the sun reaching this fragment (0 = full shadow, 1 = lit). 3x3 PCF
// with a slope-scaled bias. Anything outside the light frustum is treated as lit.
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
float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { v += a * vnoise(p); p *= 2.0; a *= 0.5; }
    return v;
}
// Procedural grass color from ground-plane XZ, with a slow wind drift.
// FUTURE: when 3D instanced blades land, keep this as the far-LOD/ground fallback
// (near = blades, far = this). The greens, clump scale, blade freq and wind speed
// below are the shared "grass look" — match the blade shader to them.
vec3 grassColor(vec2 p, float t) {
    vec2 wind  = vec2(vnoise(p * 0.25 + t * 0.35),
                      vnoise(p * 0.25 - t * 0.28)) * 0.18;
    float clump = fbm((p + wind) * 0.6);     // broad light/dark patches
    float blade = vnoise((p + wind) * 9.0);  // fine blade texture
    vec3 dark = vec3(0.11, 0.25, 0.08);
    vec3 lite = vec3(0.34, 0.53, 0.20);
    vec3 c = mix(dark, lite, clamp(clump * 0.85 + blade * 0.30, 0.0, 1.0));
    c += (blade - 0.5) * 0.06;               // micro contrast
    float dry = smoothstep(0.72, 0.88, fbm(p * 0.4 + 10.0));
    c = mix(c, vec3(0.46, 0.43, 0.20), dry * 0.35);  // dry tufts
    return c;
}

// Triplanar sample of an arbitrary map, given precomputed axis blend weights `an`.
vec3 triplanarTex(sampler2D s, vec3 p, float tile, vec3 an) {
    return texture(s, p.yz / tile).rgb * an.x
         + texture(s, p.xz / tile).rgb * an.y
         + texture(s, p.xy / tile).rgb * an.z;
}

vec3 axisBlend(vec3 p) {
    vec3 an = abs(normalize(cross(dFdx(p), dFdy(p))));
    return an / max(an.x + an.y + an.z, 1e-4);
}

// Anti-tiling: blend the same texture at two incommensurate scales. The combined
// pattern repeats only at the LCM of the two periods (far off-screen), so the obvious
// grid/diamond repeat dissolves. ~2x the samples — fine for ground.
vec3 antiTile(sampler2D s, vec3 p, float tile, vec3 an) {
    vec3 a = triplanarTex(s, p, tile,        an);
    vec3 b = triplanarTex(s, p, tile * 3.17, an);
    return mix(a, b, 0.5);
}

vec3 triplanar(vec3 p, float tile) {
    return antiTile(diffuseMap, p, tile, axisBlend(p));
}

// Blend grass/dirt/rock across the heightfield by surface slope (steep -> rock),
// with a height bias (peaks rockier) and fbm-jittered band edges so the material
// transitions look organic instead of contour-line clean. Erangel/Miramar look.
vec3 splatTerrain(vec3 p, vec3 an) {
    vec3 grassC = (grass == 1) ? grassColor(p.xz, time)
                               : antiTile(diffuseMap, p, tileSize, an);
    vec3 dirtC  = antiTile(dirtMap, p, dirtTile, an);
    vec3 rockC  = antiTile(rockMap, p, rockTile, an);

    // Slope: steep faces -> rock. The heightfield is gentle (~few deg), so amplify
    // the slope before thresholding, plus a height term for whatever peaks exist.
    float slope  = 1.0 - clamp(normalize(vNormal).y, 0.0, 1.0);  // 0 flat .. 1 vertical
    float jitter = (fbm(p.xz * 0.06) - 0.5) * 0.10;
    float s  = clamp(slope * 6.0 + jitter, 0.0, 1.0);
    float rw = smoothstep(0.30, 0.55, s);
    rw = max(rw, smoothstep(5.0, 9.0, p.y));      // exposed rock on the high ground

    // Large-scale "biome" mask (~250 m features): bare dirt fields vs lush grass,
    // so the ground reads as varied terrain even where it is near-flat.
    float region    = fbm(p.xz * 0.004);
    float dirtField = smoothstep(0.42, 0.60, region);

    float gw = (1.0 - rw) * (1.0 - dirtField);    // grass
    float dw = (1.0 - rw) * dirtField;            // dirt fields / paths
    float sum = gw + dw + rw + 1e-4;
    vec3 col = (grassC * gw + dirtC * dw + rockC * rw) / sum;

    // Macro variation: large-scale brightness drift (~30 m) so the ground doesn't
    // read as one uniform repeating carpet into the distance.
    col *= 0.82 + 0.36 * fbm(p.xz * 0.03);
    return col;
}

void main() {
    vec3 c = color;
    if (lit == 1 && splat == 1) {
        c = splatTerrain(worldPos, axisBlend(worldPos));
    } else if (lit == 1 && grass == 1) {
        c = grassColor(worldPos.xz, time);
    } else if (lit == 1 && useTexture != 0) {
        c = triplanar(worldPos, tileSize) * tint;
    }

    if (lit == 1) {
        vec3 n = (hasNormal == 1) ? normalize(vNormal)
                                  : normalize(cross(dFdx(worldPos), dFdy(worldPos)));
        vec3 L = normalize(sunDir);

        // 3-term outdoor light: warm direct sun (shadowed) + cool hemispheric sky + bounce.
        vec3 sun     = vec3(1.0, 0.96, 0.88) * max(dot(n, L), 0.0) * sunVisibility(n, L);
        vec3 ambient = mix(groundAmbient, skyZenith, n.y * 0.5 + 0.5);
        ambient     += groundAmbient * 0.25 * max(-n.y, 0.0);   // upward bounce
        vec3 lit3    = c * (sun + ambient);

        if (specular > 0.0) {
            vec3 V = normalize(eyePos - worldPos);
            vec3 H = normalize(L + V);
            float spec = pow(max(dot(n, H), 0.0), 48.0) * specular;
            lit3 += vec3(spec);
        }

        float fog = clamp(length(worldPos - eyePos) / 350.0, 0.0, 1.0);
        c = mix(lit3, skyHorizon, fog * fog);
    }
    fragColor = vec4(c, alpha);
}
