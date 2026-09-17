#version 330 core
in vec3  worldPos;
in vec3  terrainNormal;
flat in float trainingMeadow;
in vec3  vNormal;
in vec3  vColor;
in vec2  vUV;
in vec4  lightSpacePos;
in vec4  meadowField;
in float meadowCloud;
in vec3  meadowFog;

uniform vec3  eyePos;
uniform vec3  sunDir;
uniform vec3  sunColor;
uniform vec3  skyZenith;
uniform vec3  groundAmbient;
uniform float fogDist;
uniform float fogHeightAmt;
uniform float exposure;
uniform float saturation;
uniform sampler2D shadowMap;
uniform sampler2D branchTex;

out vec4 fragColor;

#include "shadow_bias.glsl"
float sunVisibility(vec3 n, vec3 L) {
    vec3 p = lightSpacePos.xyz / lightSpacePos.w * 0.5 + 0.5;
    if (p.z > 1.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return 1.0;
    float bias = shadowBias(n, L, 0.75);
    return p.z - bias > texture(shadowMap, p.xy).r ? 0.0 : 1.0;
}

vec3 grade(vec3 c) {
    c *= exposure;
    c = mix(vec3(dot(c, vec3(0.299, 0.587, 0.114))), c, saturation);
    return clamp((c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    bool card = vUV.x < -7.5;
    float eyeDistance = length(worldPos.xz - eyePos.xz);
    float cardDistance = smoothstep(12.0, 42.0, eyeDistance);
    vec3 albedo = vColor;
    if (card) {
        vec4 plant = texture(branchTex, vec2(-vUV.x - 8.0, vUV.y));
        float detail = clamp(dot(plant.rgb, vec3(.299, .587, .114)), .15, .65);
        vec3 photograph = vColor * (.90 + .3 * detail);
        if (trainingMeadow > 0.5)
            photograph = mix(photograph * vec3(1.25, 1.08, .85), photograph, meadowField.a);
        albedo = mix(photograph, meadowField.rgb, smoothstep(18.0, 48.0, eyeDistance) * .92);
    }

    vec3 V = normalize(eyePos - worldPos);
    vec3 n = normalize(vNormal);
    if (card) n = normalize(mix(n, terrainNormal, cardDistance));
    vec3 L = normalize(sunDir);

    float visible = sunVisibility(n, L) * meadowCloud;
    vec3 sun = sunColor * max(dot(n, L), 0.0) * visible;
    vec3 ambient = mix(groundAmbient, skyZenith, n.y * 0.5 + 0.5);
    float rootShade = mix(.68, 1.0, smoothstep(0.0, .7, vUV.y));
    if (vUV.x < -2.5) rootShade = mix(rootShade, 1.0, smoothstep(16.0, 44.0, eyeDistance));
    float through = pow(max(dot(-L, V), 0.0), 3.0) * .22;
    if (card) { rootShade = mix(.88, 1.0, cardDistance); through *= 1.0 - cardDistance; }
    if (trainingMeadow > 0.5) {
        through = pow(max(dot(-L, V), 0.0), 2.0) * .55 * vUV.y;
        rootShade = mix(.62, 1.0, smoothstep(0.0, .55, vUV.y));
        rootShade = mix(rootShade, 1.0, cardDistance * .7);
    }
    vec3 lit = albedo * (sun + ambient + sunColor * through * visible) * rootShade;

    float dens = 1.0 + fogHeightAmt * exp(-max(worldPos.y, 0.0) / 12.0);
    float fog = clamp(length(worldPos - eyePos) * dens / fogDist, 0.0, 1.0);
    fragColor = vec4(grade(mix(lit, meadowFog, fog * fog)), 1.0);
}
