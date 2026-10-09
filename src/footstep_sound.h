#pragma once
#include <vector>
// Temporary procedural fallback. Replaced by WAV assets at startup when present.
void buildFootstepFallback(std::vector<float>& out, bool gravel, unsigned variant);
