#pragma once
#include <glm/glm.hpp>
#include <cmath>

// Metres, Y-up. Eight 14.2 m stairwell sections on the training map.
constexpr int KHRUSH_SECTIONS = 8;
constexpr int KHRUSH_FLOORS = 5;
constexpr float KHRUSH_SECTION = 14.2f;
constexpr float KHRUSH_LENGTH = KHRUSH_SECTIONS * KHRUSH_SECTION;
constexpr float KHRUSH_DEPTH = 11.52f;
constexpr float KHRUSH_FLOOR = 2.7f;
constexpr float KHRUSH_X = 0.0f, KHRUSH_Z = -110.0f, KHRUSH_Y = 12.0f;
inline bool khrushchevkaClearance(float x, float z, float margin) {
    return fabsf(x-KHRUSH_X) < KHRUSH_LENGTH*.5f+2+margin &&
           fabsf(z-KHRUSH_Z) < KHRUSH_DEPTH*.5f+3+margin;
}
struct KhrushPart {
    glm::vec3 center, half, color;
    int section;
    bool concrete, detail, solid;
};
using KhrushSink = void (*)(const KhrushPart&, void*);
// One source of structural surfaces/openings for render export and collision.
void visitKhrushchevka(KhrushSink sink, void* context);
void visitKhrushchevkaDetails(KhrushSink sink, void* context);
void buildKhrushchevkaCollision();
inline int gKhrushBoxFirst = 0, gKhrushBoxCount = 0;
