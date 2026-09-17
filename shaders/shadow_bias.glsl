uniform mat4 lightSpace;
float shadowBias(vec3 n, vec3 L, float filterTexels) {
    float ndl = clamp(dot(n, L), 0.05, 1.0);
    float slope = min(sqrt(1.0 - ndl * ndl) / ndl, 6.0);
    float perMetre = length(vec3(lightSpace[0][0], lightSpace[1][0], lightSpace[2][0]));
    float depthPerMetre = length(vec3(lightSpace[0][2], lightSpace[1][2], lightSpace[2][2]));
    float texel = 2.0 / (perMetre * float(textureSize(shadowMap, 0).x));
    return 0.5 * depthPerMetre * texel * (filterTexels * slope + 1.0);
}
