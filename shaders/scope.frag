#version 330 core
in vec2 vNdc;
uniform float aspect;
uniform float opacity;
out vec4 fragColor;

void main() {
    vec2 p = vec2(vNdc.x * aspect, vNdc.y);
    float radius = length(p);
    float ring = smoothstep(0.815, 0.83, radius);
    float center = 1.0 - smoothstep(0.007, 0.012, radius);
    float vertical = (1.0 - smoothstep(0.001, 0.0025, abs(p.x))) * step(0.035, abs(p.y));
    float horizontal = (1.0 - smoothstep(0.001, 0.0025, abs(p.y))) * step(0.035, abs(p.x));
    float reticle = max(center, max(vertical, horizontal)) * (1.0 - ring);
    fragColor = vec4(0.01, 0.012, 0.013, opacity * max(ring, reticle * 0.95));
}
