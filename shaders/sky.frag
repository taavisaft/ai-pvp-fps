#version 330 core
// Gradient sky + soft sun disk. The world view ray is rebuilt by unprojecting the
// pixel's NDC at the near and far planes (invViewProj) and taking their difference,
// so the gradient is anchored to true world-up regardless of camera pitch/roll.
in vec2 vNdc;
uniform mat4 invViewProj;
uniform vec3 sunDir;      // direction TOWARD the sun, normalized
uniform vec3 sunColor;    // preset sun tint (disk + glow follow the mood)
uniform vec3 skyZenith;   // color overhead
uniform vec3 skyHorizon;  // color at the horizon (matches world fog)
uniform float exposure;   // same grade as basic.frag so the fog seam stays invisible
uniform float saturation;
out vec4 fragColor;

vec3 grade(vec3 c) {
    c *= exposure;
    c = mix(vec3(dot(c, vec3(0.299, 0.587, 0.114))), c, saturation);
    return clamp((c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec4 nearP = invViewProj * vec4(vNdc, -1.0, 1.0);
    vec4 farP  = invViewProj * vec4(vNdc,  1.0, 1.0);
    vec3 dir   = normalize(farP.xyz / farP.w - nearP.xyz / nearP.w);

    float up  = clamp(dir.y, 0.0, 1.0);
    vec3  col = mix(skyHorizon, skyZenith, pow(up, 0.45));

    // Sun: tight bright core + broad glow, tinted by the atmosphere preset
    // (overcast = dim grey smudge, golden hour = big warm blob).
    float s   = max(dot(dir, normalize(sunDir)), 0.0);
    float lum = clamp(dot(sunColor, vec3(0.4)), 0.0, 1.5);   // weak sun -> weak disk
    col += sunColor * pow(s, 900.0) * 1.2 * lum;
    col += sunColor * pow(s, 24.0)  * 0.25 * lum;

    fragColor = vec4(grade(col), 1.0);
}
