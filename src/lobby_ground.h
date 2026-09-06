#pragma once
#include <cmath>

// Shared lobby experiment masks. Mirrored by lobbyWear in basic.frag.
inline float lobbySmooth(float a, float b, float v) {
    float t = fmaxf(0, fminf(1, (v-a)/(b-a)));
    return t*t*(3-2*t);
}
inline float lobbyWear(float x, float z) {
    float range = (1-lobbySmooth(9,15,fabsf(z))) *
                  (1-lobbySmooth(26,32,x)) * lobbySmooth(-19,-13,x);
    float trail = 1-lobbySmooth(0.7f,2.2f,fabsf(z-19-2*sinf(x*0.10f)));
    return fmaxf(range, trail * 0.92f);
}

// Dense reference patch: feather the boundary into the existing lobby meadow.
inline float lobbyMeadow(float x, float z) {
    return (1-lobbySmooth(8,10,fabsf(x)))*(1-lobbySmooth(8,10,fabsf(z-30)));
}
