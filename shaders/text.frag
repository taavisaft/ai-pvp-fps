#version 330 core
in vec2 uv;
uniform vec3 color;
uniform float alpha;
uniform sampler2D atlas;
out vec4 fragColor;
void main() {
    if (texture(atlas, uv).r < 0.5) discard;
    fragColor = vec4(color, alpha);
}
