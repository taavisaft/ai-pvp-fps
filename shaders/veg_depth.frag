#version 330 core
// Sun-depth for vegetation: depth-only, but the branch cards are alpha cutouts —
// without this test every card would cast a solid rectangular shadow.
in vec2 vUV;
uniform sampler2D branchTex;
uniform int trainingTree;
void main() {
    float cut=trainingTree==7 ? .074 : trainingTree==1 || trainingTree==5 ? .22 : trainingTree>0 ? .30 : .42;
    if (vUV.x >= 0.0 && texture(branchTex, vUV).a < cut) discard;
}
