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

void main() {
    vec4 tex = texture(diffuseMap, vUV);
    if (alphaCutoff >= 0.0 && tex.a < alphaCutoff) discard;   // leaf cutout
    vec3 c = tex.rgb;

    vec3 n = normalize(vNormal);
    if (!gl_FrontFacing) n = -n;     // double-sided leaves: face the viewer
    vec3 L = normalize(sunDir);

    vec3 sun     = vec3(1.0, 0.96, 0.88) * max(dot(n, L), 0.0) * sunVisibility(n, L);
    vec3 ambient = mix(groundAmbient, skyZenith, n.y * 0.5 + 0.5);
    vec3 lit     = c * (sun + ambient);

    float fog = clamp(length(worldPos - eyePos) / 350.0, 0.0, 1.0);
    fragColor = vec4(mix(lit, skyHorizon, fog * fog), 1.0);
}
