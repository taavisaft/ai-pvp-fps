#include "physics.h"
#include "map.h"
#include "tree_collision.h"
#include <cstdio>
#include <cmath>
#include <chrono>
#include <cstring>

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); ++failures; } } while (0)

static void setup() {
    gTerrainMode = TERRAIN_OFF;
    gMapBoxCount = 0;
    gTrees = {{0, 0, 0, 10, 0, 1}};
    gTreeTrunkMaxRadius = 0.2f;
    gTreeColGrid.init(100, 20, 0, 10);
    gTreeColGrid.insert(0, 0, 0);
}

static void protectedTarget() {
    setup();
    GameState gs{};
    gs.usedMask = 3;
    gs.players[1].pos = {2, 0, 0};
    CHECK(spawnBullet(gs, {-2, 1, 0}, {1, 0, 0}, 0));
    Impact impact{};
    int count = 0;
    updateBullets(gs, 1.0f / 60, nullptr, nullptr, &impact, &count, 1);
    CHECK(!gs.bullets[0].active);
    CHECK(gs.players[1].hp == 100);
    CHECK(count == 1);
    CHECK(impact.pos.x < 0 && impact.pos.x > -0.21f);
    CHECK(impact.normal.x < -0.8f);
}

static bool rewindTarget(const void*, int, float, glm::vec3& pos, bool&, float&,
                         float&, float&, bool&, uint8_t&, bool& alive) {
    pos = {2, 0, 0}; alive = true; return true;
}

static void geometry() {
    setup();
    float t = -1;
    glm::vec3 n;
    const TreeInstance tree = gTrees[0];
    CHECK(segmentTreeTrunk(tree, {-100, 1, 0}, {100, 1, 0}, t, n));
    CHECK(t > 0.498f && t < 0.5f);
    CHECK(!segmentTreeTrunk(tree, {-1, 1, 0.18f}, {1, 1, 0.18f}, t, n));
    CHECK(!segmentTreeTrunk(tree, {-1, 8, 0.06f}, {1, 8, 0.06f}, t, n));
    CHECK(!segmentTreeTrunk(tree, {-1, 8.61f, 0}, {1, 8.61f, 0}, t, n));
    CHECK(!segmentTreeTrunk(tree, {-1, -0.01f, 0}, {1, -0.01f, 0}, t, n));
    CHECK(segmentTreeTrunk(tree, {0, 10, 0}, {0, 8, 0}, t, n));
    CHECK(fabsf(t - 0.7f) < 1e-5f && n.y > 0.99f);
    CHECK(segmentTreeTrunk(tree, {0, -1, 0}, {0, 1, 0}, t, n));
    CHECK(fabsf(t - 0.5f) < 1e-5f && n.y < -0.99f);
    CHECK(segmentTreeTrunk(tree, {0, 1, 0}, {1, 1, 0}, t, n) && t == 0);
    CHECK(segmentTreeTrunk(tree, {0, 1, 0}, {0, 1, 0}, t, n) && t == 0);
    CHECK(!segmentTreeTrunk(tree, {1, 1, 0}, {1, 1, 0}, t, n));
    // Probe both sides of every rendered face, after translation/scale/yaw.
    for (float yaw : {0.0f, 0.37f, 1.4f}) {
        TreeInstance rotated{51, 3, -29, 4, yaw, 1};
        auto world = [&](glm::vec3 p) {
            return glm::vec3(rotated.x, rotated.y, rotated.z) + rotated.scale *
                glm::vec3(cosf(yaw)*p.x-sinf(yaw)*p.z, p.y, sinf(yaw)*p.x+cosf(yaw)*p.z);
        };
        const float radius = (TREE_TRUNK_BASE + TREE_TRUNK_TOP) * 0.5f;
        for (int face = 0; face < TREE_TRUNK_SIDES; ++face) {
            float angle = (face + 0.5f) * 6.2831853f / TREE_TRUNK_SIDES;
            glm::vec3 outward(cosf(angle), 0, sinf(angle));
            glm::vec3 tangent(-outward.z, 0, outward.x);
            glm::vec3 center(0, TREE_TRUNK_HEIGHT * 0.5f, 0);
            for (float offset : {-0.0002f, 0.0002f}) {
                glm::vec3 q = center + outward * (radius * cosf(3.14159265f/6) + offset);
                bool hit = segmentTreeTrunk(rotated, world(q-tangent*0.001f),
                                             world(q+tangent*0.001f), t, n);
                CHECK(hit == (offset < 0));
            }
        }
    }
}

static void nearestAndRewind() {
    setup();
    // Insert farther tree first; sweep spans several cells, including a boundary.
    gTrees = {{10.05f, 0, 0, 10, 0, 1}, {0, 0, 0, 10, 0, 1}};
    gTreeColGrid.init(100, 20, 0, 10);
    for (int i = 0; i < 2; ++i) gTreeColGrid.insert(gTrees[i].x, gTrees[i].z, i);
    float t;
    glm::vec3 n;
    CHECK(sweepTreeTrunks({-30, 1, 0}, {30, 1, 0}, t, n));
    CHECK(t < 0.5f && t > 0.49f);
    CHECK(sweepTreeTrunks({9.9f, 1, -1}, {9.9f, 1, 1}, t, n));
    CHECK(!sweepTreeTrunks({-30, 1, 1}, {30, 1, 1}, t, n));
    setup();
    for (int scenario = 0; scenario < 4; ++scenario) {
        GameState gs{};
        gs.usedMask = 3;
        gs.players[1].pos = scenario == 0 ? glm::vec3(20, 0, 0) :
                           scenario == 1 ? glm::vec3(-1, 0, 0) : glm::vec3(2, 0, 0);
        gMapBoxCount = scenario == 2 ? 1 : 0;
        static const Box cover{{-1, 1, 0}, {.1f, 1, 1}};
        if (gMapBoxCount) gMapBoxes = &cover;
        // A gap through foliage, outside the thin trunk, still allows player hits.
        float z = scenario == 3 ? 0.3f : 0;
        if (scenario == 3) gs.players[1].pos.z = z;
        CHECK(spawnBullet(gs, {-2, 1, z}, {1, 0, 0}, 0));
        Impact impact{};
        int count = 0;
        updateBullets(gs, 1.0f/60, scenario == 0 ? rewindTarget : nullptr,
                      nullptr, &impact, &count, 1);
        CHECK(!gs.bullets[0].active);
        if (scenario == 1 || scenario == 3) {
            CHECK(gs.players[1].hp < 100 && count == 0);
        } else {
            CHECK(gs.players[1].hp == 100 && count == 1);
            CHECK(scenario == 0 ? impact.pos.x > -0.21f : impact.pos.x < -1);
        }
    }
}

static void benchmark() {
    setMap(MAP_PALDISKI);
    // Use the densest occupied spatial bucket in the actual deterministic map.
    const std::vector<int>* dense = nullptr;
    for (const auto& bucket : gTreeColGrid.buckets)
        if (!dense || bucket.size() > dense->size()) dense = &bucket;
    CHECK(dense && !dense->empty());
    glm::vec3 starts[MAX_BULLETS], ends[MAX_BULLETS];
    float expected[MAX_BULLETS];
    for (int i = 0; i < MAX_BULLETS; ++i) {
        const auto& tree = gTrees[(*dense)[i % dense->size()]];
        float angle = i * 2.39996323f;
        glm::vec3 direction(cosf(angle), 0, sinf(angle));
        glm::vec3 center(tree.x, tree.y + 1, tree.z + (i % 2 ? 0.5f : 0));
        starts[i] = center - direction * 3.5f;
        ends[i] = center + direction * 3.5f;
        // Independent brute-force traversal validates broadphase completeness.
        expected[i] = 2;
        for (const auto& candidate : gTrees) {
            float t; glm::vec3 n;
            if (segmentTreeTrunk(candidate, starts[i], ends[i], t, n))
                expected[i] = fminf(expected[i], t);
        }
        float t = 2; glm::vec3 n;
        sweepTreeTrunks(starts[i], ends[i], t, n);
        CHECK(fabsf(t - expected[i]) < 1e-6f);
    }
    int hits = 0;
    constexpr int rounds = 1000;
    auto start = std::chrono::steady_clock::now();
    for (int r = 0; r < rounds; ++r)
        for (int i = 0; i < MAX_BULLETS; ++i) {
            float t; glm::vec3 n;
            hits += sweepTreeTrunks(starts[i], ends[i], t, n);
        }
    double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now()-start).count();
    std::printf("trees=%zu densest_bucket=%zu queries=%d hits=%d us/query=%.3f ms/256=%.3f\n",
                gTrees.size(), dense->size(), rounds*MAX_BULLETS, hits,
                us/(rounds*MAX_BULLETS), us/rounds/1000);
}

int main(int argc, char** argv) {
    protectedTarget(); geometry(); nearestAndRewind();
    if (argc > 1 && std::strcmp(argv[1], "--bench") == 0) benchmark();
    return failures ? 1 : 0;
}

