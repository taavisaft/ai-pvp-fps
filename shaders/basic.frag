#version 330 core
in vec3 worldPos;
uniform vec3 color;
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

out vec4 fragColor;

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

vec3 triplanar(vec3 p, float tile) {
    vec3 an = abs(normalize(cross(dFdx(p), dFdy(p))));
    an /= max(an.x + an.y + an.z, 1e-4);
    vec2 uvX = p.yz / tile;
    vec2 uvY = p.xz / tile;
    vec2 uvZ = p.xy / tile;
    return texture(diffuseMap, uvX).rgb * an.x
         + texture(diffuseMap, uvY).rgb * an.y
         + texture(diffuseMap, uvZ).rgb * an.z;
}

void main() {
    vec3 c = color;
    if (lit == 1 && grass == 1) {
        c = grassColor(worldPos.xz, time);
    } else if (lit == 1 && useTexture != 0) {
        c = triplanar(worldPos, tileSize) * tint;
    }

    if (lit == 1) {
        vec3 n = normalize(cross(dFdx(worldPos), dFdy(worldPos)));
        float ndl = max(dot(n, normalize(vec3(0.5, 0.8, 0.3))), 0.0);
        c *= 0.55 + 0.45 * ndl;

        if (specular > 0.0) {
            vec3 V = normalize(eyePos - worldPos);
            float spec = pow(max(dot(n, V), 0.0), 24.0) * specular;
            c += vec3(spec);
        }

        float fog = clamp(length(worldPos - eyePos) / 250.0, 0.0, 1.0);
        c = mix(c, vec3(0.1, 0.1, 0.15), fog * fog);
    }
    fragColor = vec4(c, alpha);
}
