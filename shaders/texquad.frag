#version 330 core
in vec2 vUV;
uniform sampler2D diffuseMap;
uniform float alpha;
uniform vec3 tint;
out vec4 fragColor;
void main() {
    fragColor = vec4(texture(diffuseMap, vUV).rgb * tint, alpha);
}
