#version 330 core
in vec3 worldPos;
uniform vec3 color;
uniform float alpha;
uniform int lit;        // 1 = world pass (lighting/grid/fog), 0 = HUD
uniform vec3 eyePos;
out vec4 fragColor;
void main() {
    vec3 c = color;
    if (lit == 1) {
        // flat shading from screen-space derivatives — no normals needed
        vec3 n = normalize(cross(dFdx(worldPos), dFdy(worldPos)));
        c *= 0.55 + 0.45 * max(dot(n, normalize(vec3(0.5, 0.8, 0.3))), 0.0);
        // 1 m grid on upward ground-level faces only
        if (n.y > 0.99 && worldPos.y < 0.005) {
            vec2 g = abs(fract(worldPos.xz) - 0.5);
            c *= 1.0 - 0.15 * step(0.47, max(g.x, g.y));
        }
        float fog = clamp(length(worldPos - eyePos) / 120.0, 0.0, 1.0);
        c = mix(c, vec3(0.1, 0.1, 0.15), fog * fog);
    }
    fragColor = vec4(c, alpha);
}
