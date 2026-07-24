#version 330 core
in vec3 worldPos;
in vec3 vNormal;
in vec2 vUV;
in vec4 lightSpacePos;
uniform vec3 color;
uniform int  useFacade;   // 1 = sample diffuseMap by vUV (building facade), no triplanar
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

// Atmosphere pass (preset-driven; see Renderer::setAtmosphere).
uniform vec3  sunColor;     // direct sun tint
uniform float fogDist;      // distance-fog reach in meters
uniform float fogHeightAmt; // low-altitude haze boost (valleys fill first)
uniform float cloudAmount;  // drifting cloud-shadow strength 0..1
uniform float exposure;     // pre-tonemap exposure
uniform float saturation;   // grade saturation (1 = neutral)

uniform sampler2D shadowMap;  // sun depth, texture unit 1
uniform int useShadow;        // 1 = sample shadow map, 0 = fully lit

// Terrain slope/height splat (terrain draw only).
uniform int       splat;      // 1 = blend grass/dirt/rock by slope+height
uniform sampler2D rockMap;    // texture unit 2
uniform sampler2D dirtMap;    // texture unit 3
uniform sampler2D forestMap;  // texture unit 4: needles, moss and very low growth
uniform float     rockTile;   // world meters per rock repeat
uniform float     dirtTile;   // world meters per dirt repeat
uniform float     forestTile; // world meters per forest-floor repeat

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
    // Olive-leaning bog palette (the reference taiga meadow), not lawn green.
    vec3 dark = vec3(0.075, 0.145, 0.050);
    vec3 lite = vec3(0.225, 0.315, 0.105);
    vec3 c = mix(dark, lite, clamp(clump * 0.85 + blade * 0.30, 0.0, 1.0));
    c += (blade - 0.5) * 0.045;              // micro contrast
    float dry = smoothstep(0.55, 0.85, fbm(p * 0.4 + 10.0));
    c = mix(c, vec3(0.36, 0.32, 0.14), dry * 0.45);  // dry straw patches
    return c;
}

// Cloud shadows: two octaves of drifting value noise over the ground plane, dimming
// the direct sun where a cloud passes. Same math in foliage.frag/inst.frag so every
// surface darkens together (the Arma "cloud sweeping over a field" read).
float cloudShadow(vec2 xz, float t) {
    if (cloudAmount <= 0.0) return 1.0;
    float cl = vnoise(xz * 0.010 + t * 0.010) * 0.65
             + vnoise(xz * 0.027 - t * 0.013) * 0.35;
    return 1.0 - cloudAmount * (1.0 - smoothstep(0.35, 0.72, cl));
}

// Grade: exposure -> saturation -> ACES filmic curve (Narkowicz approximation).
// Applied to lit world pixels only (HUD stays raw); sky.frag runs the same grade
// so the fog-to-horizon seam stays invisible.
vec3 grade(vec3 c) {
    c *= exposure;
    c = mix(vec3(dot(c, vec3(0.299, 0.587, 0.114))), c, saturation);
    return clamp((c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14), 0.0, 1.0);
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
// Far ground: the two scales interfere into a moire that crawls with every camera
// step (mips can't help — the interference is between the two lookups, not inside
// one). gFar (0 near .. 1 far, set in main) fades the detail out to a band-limited
// macro sample so distant terrain holds still.
float gFar = 0.0;
vec3 antiTile(sampler2D s, vec3 p, float tile, vec3 an) {
    vec3 a = triplanarTex(s, p, tile,        an);
    vec3 b = triplanarTex(s, p, tile * 3.17, an);
    vec3 fine = mix(a, b, 0.5);
    if (gFar <= 0.0) return fine;
    vec3 macro = textureLod(s, p.xz / (tile * 23.0), 4.0).rgb;
    return mix(fine, macro, gFar);
}

vec3 triplanar(vec3 p, float tile) {
    return antiTile(diffuseMap, p, tile, axisBlend(p));
}

// Exact GLSL port of pineForestBiome() in terrain.h (same integer hash), so the
// textured forest floor sits precisely under the scattered spruce stands.
float terrHash(int x, int z) {
    uint n = uint(x * 374761393 + z * 668265263);
    n = (n ^ (n >> 13)) * 1274126177u;
    n ^= (n >> 16);
    return float(n & 0x7fffffffu) / 2147483647.0;
}
float terrVNoise(vec2 p) {
    vec2 f = floor(p);
    int xi = int(f.x), zi = int(f.y);
    vec2 t = p - f;
    t = t * t * (3.0 - 2.0 * t);
    float a = terrHash(xi, zi),     b = terrHash(xi + 1, zi);
    float c = terrHash(xi, zi + 1), d = terrHash(xi + 1, zi + 1);
    return mix(mix(a, b, t.x), mix(c, d, t.x), t.y);
}
float pineBiome(vec2 p) {
    float f = terrVNoise(p * 0.0016 + vec2(91.0, 47.0)) * 0.7
            + terrVNoise(p * 0.006) * 0.3;
    float t = clamp((f - 0.40) / 0.13, 0.0, 1.0);
    float base = t * t * (3.0 - 2.0 * t);
    // Keep in sync with pineForestBiome() in terrain.h (medium clump layer).
    float c = terrVNoise(p * 0.008 + vec2(13.0, 77.0));
    float u = clamp((c - 0.58) / 0.09, 0.0, 1.0);
    float clump = u * u * (3.0 - 2.0 * u);
    return max(base, clump * 0.85);
}

// Blend grass/dirt/rock across the heightfield by surface slope (steep -> rock),
// with a height bias (peaks rockier) and fbm-jittered band edges so the material
// transitions look organic instead of contour-line clean. Erangel/Miramar look.
vec3 splatTerrain(vec3 p, vec3 an) {
    vec3 grassC = (grass == 1) ? grassColor(p.xz, time)
                               : antiTile(diffuseMap, p, tileSize, an) * tint;
    vec3 dirtC  = antiTile(dirtMap, p, dirtTile, an);
    vec3 rockC  = antiTile(rockMap, p, rockTile, an);
    // Slope: steep faces -> rock. The heightfield is gentle (~few deg), so amplify
    // the slope before thresholding, plus a height term for whatever peaks exist.
    float slope  = 1.0 - clamp(normalize(vNormal).y, 0.0, 1.0);  // 0 flat .. 1 vertical
    float jitter = (fbm(p.xz * 0.06) - 0.5) * 0.10;
    float s  = clamp(slope * 6.0 + jitter, 0.0, 1.0);
    float rw = smoothstep(0.30, 0.55, s);
    rw = max(rw, smoothstep(115.0, 175.0, p.y));  // bare rock on the mountain slopes
    // (band sits above the ~85 m playable ridge tops so they stay grassy)

    // Large-scale "biome" mask (~250 m features): bare dirt fields vs lush grass,
    // so the ground reads as varied terrain even where it is near-flat.
    // Kept rare and partial: the reference bog meadows are olive grass nearly
    // everywhere — dirt shows as worn patches, not sand flats.
    float region    = fbm(p.xz * 0.004);
    float dirtField = smoothstep(0.52, 0.70, region) * 0.35;

    float gw = (1.0 - rw) * (1.0 - dirtField);    // grass
    float dw = (1.0 - rw) * dirtField;            // dirt fields / paths
    float sum = gw + dw + rw + 1e-4;
    // Dirt pulled toward olive so worn patches sit inside the bog's green, not on it.
    vec3 col = (grassC * gw + dirtC * vec3(0.86, 0.90, 0.66) * dw + rockC * rw) / sum;
    float pine = pineBiome(p.xz) * (1.0 - rw);
    if (pine > 0.001) {
        // Terrain is predominantly upward-facing, so two XZ samples replace another
        // triplanar set. This branch means non-pine environments pay no texture cost.
        vec3 forestA = texture(forestMap, p.xz / forestTile).rgb;
        vec3 forestB = texture(forestMap, p.xz / (forestTile * 3.17)).rgb;
        vec3 forestF = mix(forestA, forestB, 0.46);
        if (gFar > 0.0)   // same far de-moire as antiTile
            forestF = mix(forestF, textureLod(forestMap, p.xz / (forestTile * 23.0), 4.0).rgb, gFar);
        vec3 forestC = forestF * vec3(0.72, 0.80, 0.66);
        col = mix(col, forestC, pine);
    }

    // Beach: below ~1.2 m the ground fades to wet sand (tinted dirt), continuing
    // under the waterline so the shallows read as seabed through the water plane.
    // Narrow band, muddy-olive tint: inland river bogs sit near sea level too, and
    // they must read as wet meadow, not desert (the pale tan flooded every valley).
    float sand = 1.0 - smoothstep(0.25, 0.95, p.y);
    // Coast-only: the shoreline lives far west (x < ~-350). Inland low ground is
    // river bog and stays green — without this gate every valley floor went tan.
    sand *= 1.0 - smoothstep(-600.0, -350.0, p.x);
    col = mix(col, dirtC * vec3(0.98, 0.96, 0.72), sand);

    // Macro variation: large-scale brightness drift (~30 m) so the ground doesn't
    // read as one uniform repeating carpet into the distance.
    col *= 0.82 + 0.36 * fbm(p.xz * 0.03);

    // Snowcaps on the vista mountains: altitude band with a noise-ragged snowline,
    // thinning on steep faces so dark rock ribs streak through (the Altai look).
    float snowLine = 210.0 + (fbm(p.xz * 0.01) - 0.5) * 70.0;
    float snow = smoothstep(snowLine, snowLine + 60.0, p.y);
    snow *= 0.35 + 0.65 * smoothstep(0.35, 0.75, normalize(vNormal).y);
    col = mix(col, vec3(0.93, 0.95, 0.99), snow);
    return col;
}

void main() {
    gFar = (lit == 1) ? smoothstep(150.0, 550.0, length(worldPos - eyePos)) : 0.0;
    vec3 c = color;
    if (lit == 1 && useFacade == 1) {
        vec4 texel = texture(diffuseMap, vUV);
        if (texel.a < 0.03) discard;
        c = texel.rgb * tint;   // UV facade or transparent environment decal
    } else if (lit == 1 && splat == 1) {
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

        // 3-term outdoor light: direct sun (shadowed + cloud-dimmed) + hemispheric
        // sky + bounce. Sun tint comes from the atmosphere preset.
        vec3 sun     = sunColor * max(dot(n, L), 0.0)
                     * sunVisibility(n, L) * cloudShadow(worldPos.xz, time);
        vec3 ambient = mix(groundAmbient, skyZenith, n.y * 0.5 + 0.5);
        ambient     += groundAmbient * 0.25 * max(-n.y, 0.0);   // upward bounce
        vec3 lit3    = c * (sun + ambient);

        if (specular > 0.0) {
            vec3 V = normalize(eyePos - worldPos);
            vec3 H = normalize(L + V);
            float spec = pow(max(dot(n, H), 0.0), 48.0) * specular;
            lit3 += vec3(spec);
        }

        // Distance fog, denser at low altitude so valleys fill with haze while
        // hilltops poke out (DayZ morning). Height term uses the fragment's y.
        float dens = 1.0 + fogHeightAmt * exp(-max(worldPos.y, 0.0) / 12.0);
        float fog  = clamp(length(worldPos - eyePos) * dens / fogDist, 0.0, 1.0);
        c = grade(mix(lit3, skyHorizon, fog * fog));
    }
    float outAlpha = alpha;
    if (lit == 1 && useFacade == 1) outAlpha *= texture(diffuseMap, vUV).a;
    fragColor = vec4(c, outAlpha);
}
