uniform float hazeCool;
vec3 fogColor(vec3 viewDir) {
    vec2 view = normalize(viewDir.xz + vec2(1e-5));
    vec2 sun = normalize(sunDir.xz + vec2(1e-5));
    float toward = pow(max(dot(view, sun), 0.0), 2.0);
    vec3 cool = skyZenith * 0.50 + skyHorizon * 0.42 + vec3(0.03, 0.06, 0.11);
    return mix(skyHorizon, mix(cool, skyHorizon, toward), hazeCool);
}
