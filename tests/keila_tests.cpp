#include "map.h"
#include "physics.h"
#include <cstdio>

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); ++failures; } } while (0)

int main() {
    CHECK(mapFromName("keila", MAP_PALDISKI) == MAP_KEILA);
    setMap(MAP_KEILA);
    CHECK(gArenaHalf == KEILA_HALF);
    CHECK(gMapSpawnCount >= 16);
    float h0 = terrainHeight(0, 0);
    CHECK(h0 > 20.0f && h0 < 50.0f);
    CHECK(fabsf(terrainHeight(5000, 0) - terrainHeight(1024, 0)) < 1e-4f);

    for (int i = 0; i < gMapSpawnCount; i++) {
        Player p{};
        p.pos = gMapSpawns[i];
        p.pos.y = terrainHeight(p.pos.x, p.pos.z);
        glm::vec3 before = p.pos;
        InputState in{};
        movePlayer(p, in, 1.0f / 60);
        CHECK(glm::length(glm::vec2(p.pos.x - before.x, p.pos.z - before.z)) < 0.01f);
        CHECK(fabsf(p.pos.y - before.y) < 0.05f);
    }

    const KeilaBuilding* target = nullptr;
    int targetIndex = -1;
    for (int i = 0; i < KEILA_BUILDING_COUNT && !target; i++) {
        const float* p = KEILA_BUILDING_XZ + (size_t)KEILA_BUILDINGS[i].first * 2;
        float dx = p[2] - p[0], dz = p[3] - p[1];
        if (fabsf(p[0]) < 300 && fabsf(p[1]) < 300 && dx * dx + dz * dz > 64.0f) {
            target = &KEILA_BUILDINGS[i];
            targetIndex = i;
        }
    }
    CHECK(target != nullptr);
    if (target) {
        const float* p = KEILA_BUILDING_XZ + (size_t)target->first * 2;
        float dx = p[2] - p[0], dz = p[3] - p[1], len = sqrtf(dx * dx + dz * dz);
        glm::vec2 mid(0.5f * (p[0] + p[2]), 0.5f * (p[1] + p[3]));
        glm::vec2 out(dz / len, -dx / len);
        glm::vec2 start = mid + out * 3.0f;
        Player pl{};
        pl.pos = {start.x, terrainHeight(start.x, start.y), start.y};
        InputState in{};
        in.w = true;
        in.yaw = glm::degrees(atan2f(-out.y, -out.x));
        for (int i = 0; i < 180; i++) movePlayer(pl, in, 1.0f / 60);
        float side = (pl.pos.x - mid.x) * out.x + (pl.pos.z - mid.y) * out.y;
        CHECK(side > 0.35f && side < 0.6f);

        float eye = terrainHeight(start.x, start.y) + 1.5f;
        glm::vec3 a(start.x, eye, start.y), b(mid.x - out.x * 2.0f, eye, mid.y - out.y * 2.0f);
        float t = 0;
        glm::vec3 n;
        CHECK(keilaSweep(a, b, t, n));
        CHECK(fabsf(t - 0.6f) < 0.02f);
        CHECK(glm::dot(glm::vec2(n.x, n.z), out) > 0.99f);

        glm::vec2 in2 = mid - out * 1.0f;
        float top = keilaBuildingTop(targetIndex);
        CHECK(keilaSweep({in2.x, top + 5, in2.y}, {in2.x, top - 1, in2.y}, t, n));
        CHECK(n.y > 0.99f);
        CHECK(keilaRoofUnder(in2.x, in2.y, top + 1, 0.0f) == top);
        CHECK(keilaRoofUnder(in2.x, in2.y, top - 1, 0.0f) == 0.0f);
    }

    setMap(MAP_PALDISKI);
    glm::vec3 n;
    float t;
    CHECK(!keilaSweep({0, 30, 0}, {100, 30, 0}, t, n));
    if (failures) return 1;
    std::puts("keila ok");
    return 0;
}
