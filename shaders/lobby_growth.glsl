// Visual-only training-area field. Keep in sync with src/lobby_growth.h.
float growthHash(ivec2 p) {
    uint n=uint(p.x)*374761393u+uint(p.y)*668265263u;
    n=(n^(n>>13))*1274126177u; n^=n>>16;
    return float(n&0x7fffffffu)/2147483647.0;
}
float growthNoise(vec2 p) {
    ivec2 i=ivec2(floor(p)); vec2 f=fract(p); f=f*f*(3.0-2.0*f);
    return mix(mix(growthHash(i),growthHash(i+ivec2(1,0)),f.x),
               mix(growthHash(i+ivec2(0,1)),growthHash(i+ivec2(1,1)),f.x),f.y);
}
float lobbyGrowth(vec2 p) {
    float warp=growthNoise(p*.025+vec2(17,-9))*1.5;
    float g=.75*growthNoise(p*.075+vec2(warp,-warp))
            +.25*growthNoise(p*.22+vec2(5,0));
    return smoothstep(.30,.68,g);
}
vec3 growthColorOf(float growth) {
    return mix(vec3(.34,.35,.16),vec3(.19,.275,.095),growth);
}
vec3 fieldColor(vec2 p,float growth) {
    float dry=smoothstep(.36,.70,growthNoise(p*.008+vec2(73,-21)));
    return mix(growthColorOf(growth),vec3(.36,.37,.18),dry*.55);
}
vec3 lobbyGrowthColor(vec2 p) {
    return growthColorOf(lobbyGrowth(p));
}
