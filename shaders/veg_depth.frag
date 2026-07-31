#version 330 core
// Sun-depth for vegetation: depth-only, but the branch cards are alpha cutouts —
// without this test every card would cast a solid rectangular shadow.
in vec2 vUV;
uniform sampler2D branchTex;
void main() {
    if (vUV.x >= 0.0 && texture(branchTex, vUV).a < 0.42) discard;
}
