#pragma once
#include <cmath>

// Mirrored in the lit/depth vegetation shaders. The extra .06 preserves every
// plant in the near zone while providing a continuous fade for distant plants.
inline float meadowDensity(float distance) {
    float t=fmaxf(0,fminf(1,(distance-6.0f)/22.0f));
    t=t*t*(3-2*t);
    return 1.06f+(0.22f-1.06f)*t;
}
inline float meadowRank(float phase) {
    float value=phase*13.37f;
    return value-floorf(value);
}
