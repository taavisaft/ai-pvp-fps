#version 330 core
layout(location = 0) in vec3 aPos;        // clump mesh: x,z in [-0.5,0.5], y in [0,1]
layout(location = 1) in vec2 aUV;         // 0..1 within one atlas column
layout(location = 2) in vec3 iPos;        // per-instance clump base (on the terrain)
layout(location = 3) in vec2 iYawScale;   // per-instance yaw (rad), uniform scale
layout(location = 4) in vec3 iNormal;     // terrain normal at the clump (lighting)

uniform mat4  view;
uniform mat4  proj;
uniform mat4  lightSpace;
uniform float time;
uniform int   atlas4x4;    // 1 = close-grass asset (4x4 tiles), 0 = baked 4x1 strip

out vec3  worldPos;
out vec3  vNormal;     // terrain normal — grass shades like the ground under it
out vec2  vUV;         // final atlas UV (tile chosen here)
out float vRootT;      // within-tile v: 1 = root baseline, 0 = blade tip
out float vTintSeed;
out vec4  lightSpacePos;

float ghash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

void main() {
    float yaw = iYawScale.x, scale = iYawScale.y;
    float c = cos(yaw), s = sin(yaw);
    mat3 rot = mat3(c, 0.0, -s,   0.0, 1.0, 0.0,   s, 0.0, c);

    vec3 lp = aPos * scale;
    // Wind: bend grows quadratically toward the blade tip (base stays planted).
    // Same clock as the trees' sway so field and canopy gust together, plus a
    // faster ripple so close grass feels alive.
    float bend = lp.y * lp.y / max(scale, 0.001);
    float gust = sin(time * 1.3 + iPos.x * 0.5 + iPos.z * 0.5)
               + 0.5 * sin(time * 3.1 + iPos.x * 1.7 - iPos.z * 1.1);
    lp.x += gust * 0.06 * bend;
    lp.z += cos(time * 1.1 + iPos.x * 0.4 - iPos.z * 0.3) * 0.05 * bend;

    vec3 wp = iPos + rot * lp;
    worldPos      = wp;
    vNormal       = iNormal;
    // Tile pick, hashed from position. Asset atlas: rows 0-1 (short olive meadow +
    // tall mixed) carry the dense ring; baked fallback has 4 columns in one row.
    if (atlas4x4 == 1) {
        float t   = floor(ghash(iPos.xz) * 8.0);          // tiles 0..7
        vec2 tile = vec2(mod(t, 4.0), floor(t / 4.0));
        vUV = (aUV + tile) * 0.25;
    } else {
        float t = floor(ghash(iPos.xz) * 4.0);
        vUV = vec2((aUV.x + t) * 0.25, aUV.y);
    }
    vRootT        = aUV.y;
    vTintSeed     = ghash(iPos.zx + 7.31);
    lightSpacePos = lightSpace * vec4(wp, 1.0);
    gl_Position   = proj * view * vec4(wp, 1.0);
}
