// Shared world-space meadow albedo: sampled by both terrain and grass cards.
float meadowHash(vec2 p) {
    p=fract(p*vec2(123.34,456.21)); p+=dot(p,p+45.32);
    return fract(p.x*p.y);
}
float meadowNoise(vec2 p) {
    vec2 i=floor(p), f=fract(p); f=f*f*(3.0-2.0*f);
    return mix(mix(meadowHash(i),meadowHash(i+vec2(1,0)),f.x),
               mix(meadowHash(i+vec2(0,1)),meadowHash(i+vec2(1,1)),f.x),f.y);
}
vec3 meadowSurface(vec2 p) {
    float broad=meadowNoise(p*.075);
    float patch=meadowNoise(p*.27+vec2(meadowNoise(p*.09)*2.0));
    return mix(vec3(.12,.15,.06),vec3(.19,.225,.095),
               .3+.35*broad+.15*patch);
}

// Static fine strands are filtered out before their footprint becomes subpixel.
vec3 meadowGround(vec2 q) {
    vec2 a=vec2(q.x*.8+q.y*.6,-q.x*.6+q.y*.8)*vec2(4,28);
    vec2 b=vec2(q.x*.6-q.y*.8,q.x*.8+q.y*.6)*vec2(5,24);
    float footprint=max(length(fwidth(a)),length(fwidth(b)));
    float detail=1.0-smoothstep(.35,1.5,footprint);
    float strands=mix(.5,(meadowNoise(a)+meadowNoise(b))*.5,detail);
    return meadowSurface(q)*(.90+.20*strands);
}
