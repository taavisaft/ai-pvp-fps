// Habitat boundaries mirror trainingForest() and forestFloorCover() on the CPU.
float trainingWoodland(vec2 p) {
    float patch=terrVNoise(p*vec2(.015,.013)+vec2(19,-7));
    float density=max(smoothstep(290.0,520.0,p.y)*.95,
                      smoothstep(90.0,230.0,abs(p.x))*.85)*smoothstep(.38,.52,patch);
    vec2 pond=p/vec2(44,30)-vec2(0,184.0/30.0);
    float r=length(pond)*(1.0+.06*sin(p.x*.12+p.y*.09));
    float ring=(1.0-smoothstep(1.5,2.4,r))*smoothstep(.95,1.3,r);
    float clump=smoothstep(.50,.64,terrVNoise(p*.045+vec2(3,11)));
    return max(density,ring*.55*clump*smoothstep(173.0,202.0,p.y));
}
float woodlandCover(vec2 p) {
    if(splat==2) return smoothstep(.10,.70,trainingWoodland(p));
    float habitat=smoothstep(1.6,2.6,worldPos.y)*(1.0-smoothstep(90.0,95.0,worldPos.y));
    return smoothstep(.10,.70,pineBiome(p))*habitat;
}

// Four materials share one texture binding. Insets and capped mip levels keep
// neighboring atlas quadrants out of the filter footprint. World-space warping
// breaks up repeats without extra geometry or texture lookups.
vec3 floorTile(vec2 p,vec2 quadrant) {
    vec2 uv=1.0-abs(1.0-mod(p/.9,2.0));
    vec2 size=vec2(textureSize(forestMap,0));
    vec2 dx=dFdx(p/.9)*.5, dy=dFdy(p/.9)*.5;
    float lod=clamp(log2(max(max(length(dx*size),length(dy*size)),1.0)),0.0,6.0);
    vec2 pad=vec2(1.5*exp2(ceil(lod)))/size;
    vec2 lo=quadrant*.5+pad, hi=(quadrant+1.0)*.5-pad;
    return textureLod(forestMap,mix(lo,hi,uv),lod).rgb;
}
vec3 woodlandFloor(vec3 p) {
    float broad=terrVNoise(p.xz*.035+vec2(13,41));
    float damp=terrVNoise(p.xz*.13+vec2(-8,17));
    float broken=terrVNoise(p.xz*.65+vec2(31,-4));
    vec2 warp=vec2(broad,damp)*.45;
    vec2 rotated=vec2(p.x*.8-p.z*.6,p.x*.6+p.z*.8)+warp;
    // loadTexture flips the image: its top row occupies upper GL quadrants.
    vec3 moss=floorTile(p.xz+warp,vec2(0,1));
    vec3 needles=floorTile(rotated,vec2(1,1));
    vec3 leaves=floorTile(p.xz*vec2(-1,1)+warp,vec2(0,0));
    vec3 soil=floorTile(rotated*1.17,vec2(1,0));
    // Irregular metre-scale carpets, dry litter islands and sparse exposed soil.
    float mossWeight=smoothstep(.32,.64,.65*damp+.25*broad+.10*broken);
    float leafWeight=smoothstep(.58,.76,terrVNoise(p.xz*.075+vec2(53,9)))*.7;
    float bare=smoothstep(.62,.80,.7*broken+.3*(1.0-damp))*.72;
    vec3 litter=mix(needles,leaves,leafWeight);
    vec3 floor=mix(litter,moss,mossWeight);
    return mix(floor,soil,bare)*(.88+.18*broad);
}
