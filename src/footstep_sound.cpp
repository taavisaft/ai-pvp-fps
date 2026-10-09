#include "footstep_sound.h"
#include <cmath>
#include <cstdint>

void buildFootstepFallback(std::vector<float>& out, bool gravel, unsigned variant) {
    constexpr float sr = 44100.0f;
    constexpr float pi = 3.14159265358979323846f;
    uint32_t state = 0x9e3779b9u ^ (variant+1)*7919u ^ (gravel ? 0xabcdefu : 0u);
    auto noise = [&]() {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return float(state & 0xffffu)/32767.5f-1.0f;
    };
    out.assign(size_t(sr*(gravel ? 0.24f : 0.20f)), 0.0f);
    float low = 0, rustle = 0, grit = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        float t = float(i)/sr;
        float n = noise();
        low += (n-low)*0.025f;
        rustle += (n-rustle)*(gravel ? 0.36f : 0.12f);
        // Heel compression then a quieter toe scrape; scattered stone contacts
        // distinguish gravel from the softer, filtered vegetation rustle.
        float attack = fminf(t/0.004f,1.0f);
        float heel = attack*expf(-t*45.0f);
        float toe = t > 0.035f ? (t-0.035f)*45.0f*expf(-(t-0.035f)*30.0f) : 0;
        if (gravel && noise() > 0.985f) grit = noise()*0.22f;
        grit *= 0.93f;
        float body = low*1.6f + sinf(2*pi*(68+variant*3)*t)*0.08f;
        float sound = body*heel + rustle*(heel*0.22f+toe*(gravel ? 0.42f : 0.30f));
        sound += gravel ? grit*expf(-t*13) : 0;
        float fade = fminf(float(out.size()-i)/441.0f,1.0f);
        out[i] = sound*fade;
    }
}
