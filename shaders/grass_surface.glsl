#include "meadow_surface.glsl"

// Static fine strands are filtered out before their footprint becomes subpixel.
vec3 meadowGround(vec2 q) {
    vec2 a=vec2(q.x*.8+q.y*.6,-q.x*.6+q.y*.8)*vec2(4,28);
    vec2 b=vec2(q.x*.6-q.y*.8,q.x*.8+q.y*.6)*vec2(5,24);
    float footprint=max(length(fwidth(a)),length(fwidth(b)));
    float detail=1.0-smoothstep(.35,1.5,footprint);
    float strands=mix(.5,(meadowNoise(a)+meadowNoise(b))*.5,detail);
    return meadowSurface(q)*(.90+.20*strands);
}
