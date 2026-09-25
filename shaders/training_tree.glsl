uniform int trainingTree;
// Crown-only variation, shared by lit and shadow passes. The trunk is fixed.
vec3 trainingTreeShape(vec3 p, vec2 uv, vec4 instance, float interior) {
    if(trainingTree==0 || uv.y<0.0) return p;
    float seed=fract(sin(dot(instance.xz,vec2(12.9898,78.233)))*43758.5453);
    float width=.88+.18*seed+.035*sin(p.y*19.0+seed*6.28);
    // Saplings retain low foliage; mature crowns start at different heights.
    float mature=smoothstep(6.0,11.0,instance.w);
    if(trainingTree>1) {
        float crownBase=.11+interior*mature*(.30+.10*seed);
        p.y=crownBase+(p.y-.11)*(1.0-crownBase)/.89;
        p.xz*=width*(1.0-.12*interior*mature);
        return p;
    }
    float base=mix(.09,.22,smoothstep(3.5,8.0,instance.w))+.055*(seed-.5)+.16*interior*mature;
    p.y=base+(p.y-.17)*(1.0-base)/.83;
    // Each instance gets broad and fine crown lobes. Apply the same continuous
    // warp to wood and cards so their attachment points remain together.
    float a=atan(p.z,p.x);
    float lobe=1.0+.12*sin(a*3.0+seed*13.0+p.y*8.0)
                  +.065*sin(a*7.0-seed*19.0+p.y*17.0);
    float reach=length(p.xz);
    p.xz*=width*lobe;
    p.y+=.014*sin(a*4.0+seed*11.0+p.y*12.0)
         *smoothstep(.02,.16,reach)*(1.0-p.y);
    return p;
}

// Shared spatial deformation keeps branch junctions and attached spray stems
// together. Per-card flex used to move each disconnected piece independently.
float trainingWindFlex(vec3 p,vec2 uv,float flex) {
    if(trainingTree>0 && uv.y>=0.0)
        return max(0.0,length(p.xz)-.025)*.28;
    return flex;
}
