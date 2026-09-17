#include "vegetation.h"
#include "map.h"
#include <algorithm>
#include <cmath>

static constexpr int LANDSCAPE_SIDE = 1024;

void Vegetation::bakeLandscape(const glm::vec3& sunDir) {
    landscapeSun = sunDir;
    const float texel = 2.0f * LOBBY_HALF / LANDSCAPE_SIDE;
    landscapePixels.assign((size_t)LANDSCAPE_SIDE * LANDSCAPE_SIDE * 2, 0);
    auto splat = [&](int channel, float cx, float cz, float radius, float strength) {
        float u = (cx + LOBBY_HALF) / texel, v = (cz + LOBBY_HALF) / texel, r = radius / texel + .75f;
        int x0 = std::max(0, (int)floorf(u - r)), x1 = std::min(LANDSCAPE_SIDE - 1, (int)ceilf(u + r));
        int z0 = std::max(0, (int)floorf(v - r)), z1 = std::min(LANDSCAPE_SIDE - 1, (int)ceilf(v + r));
        for (int z = z0; z <= z1; ++z) for (int x = x0; x <= x1; ++x) {
            float dx = x + .5f - u, dz = z + .5f - v;
            float t = std::min(1.0f, std::max(0.0f, (sqrtf(dx * dx + dz * dz) / r - .45f) / .55f));
            uint8_t value = (uint8_t)(255.0f * strength * (1 - t * t * (3 - 2 * t)));
            uint8_t& pixel = landscapePixels[((size_t)z * LANDSCAPE_SIDE + x) * 2 + channel];
            pixel = std::max(pixel, value);
        }
    };
    glm::vec2 flat(sunDir.x, sunDir.z);
    float flatLength = glm::length(flat);
    glm::vec2 away = flatLength > .001f ? -flat / flatLength : glm::vec2(0);
    float slope = std::max(sunDir.y, .08f) / std::max(flatLength, .001f);
    for (const Tree& t : trees) {
        float crown = t.scale * trainingTreeWidth(t.type) * .45f;
        splat(0, t.pos.x, t.pos.z, crown, 1.0f);
        float reach = std::min(t.scale / slope, 120.0f);
        for (float s = reach * .25f; s <= reach; s += texel * 1.5f) {
            float along = s / reach;
            splat(1, t.pos.x + away.x * s, t.pos.z + away.y * s,
                  crown * (1.0f - .35f * along), .85f - .45f * along);
        }
    }
    if (!landscapeTex) {
        glGenTextures(1, &landscapeTex);
        glBindTexture(GL_TEXTURE_2D, landscapeTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, landscapeTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, LANDSCAPE_SIDE, LANDSCAPE_SIDE, 0, GL_RG,
                 GL_UNSIGNED_BYTE, landscapePixels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Vegetation::destroyLandscape() {
    if (landscapeTex) glDeleteTextures(1, &landscapeTex);
    landscapeTex = 0;
    landscapeSun = glm::vec3(0);
}
