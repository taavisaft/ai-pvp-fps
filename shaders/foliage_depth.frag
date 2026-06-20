#version 330 core
in vec2 vUV;
uniform sampler2D diffuseMap;
uniform float alphaCutoff;   // <0 = opaque; else discard alpha < cutoff (leaf gaps)
void main() {
    if (alphaCutoff >= 0.0 && texture(diffuseMap, vUV).a < alphaCutoff) discard;
    // depth-only: no color output
}
