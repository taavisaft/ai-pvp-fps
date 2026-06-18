#version 330 core
// Gradient sky + soft sun disk. The world view ray is rebuilt by unprojecting the
// pixel's NDC at the near and far planes (invViewProj) and taking their difference,
// so the gradient is anchored to true world-up regardless of camera pitch/roll.
in vec2 vNdc;
uniform mat4 invViewProj;
uniform vec3 sunDir;      // direction TOWARD the sun, normalized
uniform vec3 skyZenith;   // color overhead
uniform vec3 skyHorizon;  // color at the horizon (matches world fog)
out vec4 fragColor;

void main() {
    vec4 nearP = invViewProj * vec4(vNdc, -1.0, 1.0);
    vec4 farP  = invViewProj * vec4(vNdc,  1.0, 1.0);
    vec3 dir   = normalize(farP.xyz / farP.w - nearP.xyz / nearP.w);

    float up  = clamp(dir.y, 0.0, 1.0);
    vec3  col = mix(skyHorizon, skyZenith, pow(up, 0.45));

    // Sun: tight bright core + broad warm glow.
    float s    = max(dot(dir, normalize(sunDir)), 0.0);
    col += vec3(1.0, 0.95, 0.85) * pow(s, 900.0) * 1.2;   // disk
    col += vec3(1.0, 0.85, 0.6)  * pow(s, 24.0)  * 0.25;  // glow

    fragColor = vec4(col, 1.0);
}
