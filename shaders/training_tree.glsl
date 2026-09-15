uniform int trainingTree;
// Crown-only variation, shared by lit and shadow passes. The trunk is fixed.
vec3 trainingTreeShape(vec3 p, vec2 uv, vec4 instance) {
    if(trainingTree==0 || uv.y<0.0) return p;
    float seed=fract(sin(dot(instance.xz,vec2(12.9898,78.233)))*43758.5453);
    float width=.88+.18*seed+.035*sin(p.y*19.0+seed*6.28);
    // Saplings retain low foliage; mature crowns start at different heights.
    if(trainingTree>1) { p.xz*=width; return p; }
    float base=mix(.09,.22,smoothstep(3.5,8.0,instance.w))+.055*(seed-.5);
    p.y=base+(p.y-.17)*(1.0-base)/.83;
    p.xz*=width;
    return p;
}
