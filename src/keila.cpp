#include "keila.h"
#include "spatial.h"
#include <cmath>
#include <vector>

static std::vector<KeilaWall> gKeilaWalls;
static std::vector<float>     gKeilaBase, gKeilaTop;
static SpatialGrid            gKeilaWallGrid, gKeilaBuildingGrid;

static constexpr int   GRID_CELLS  = 128;
static constexpr float WALL_SAMPLE = 4.0f;
static constexpr float QUERY_PAD   = 2.1f;

float keilaHeight(float x, float z) {
    float fx = (x + KEILA_WORLD_HALF) / KEILA_GRID_STEP;
    float fz = (z + KEILA_WORLD_HALF) / KEILA_GRID_STEP;
    const float last = (float)(KEILA_GRID - 1) - 0.001f;
    fx = fx < 0.0f ? 0.0f : (fx > last ? last : fx);
    fz = fz < 0.0f ? 0.0f : (fz > last ? last : fz);
    int ix = (int)fx, iz = (int)fz;
    float tx = fx - (float)ix, tz = fz - (float)iz;
    const uint16_t* row0 = KEILA_HEIGHT_CM + (size_t)iz * KEILA_GRID + ix;
    const uint16_t* row1 = row0 + KEILA_GRID;
    float a = (float)row0[0] + ((float)row0[1] - (float)row0[0]) * tx;
    float b = (float)row1[0] + ((float)row1[1] - (float)row1[0]) * tx;
    return KEILA_HEIGHT_BASE + (a + (b - a) * tz) * 0.01f;
}

bool keilaBare(float x, float z) {
    int ix = (int)floorf(x + KEILA_HALF), iz = (int)floorf(z + KEILA_HALF);
    if (ix < 0 || iz < 0 || ix >= KEILA_BARE_SIDE || iz >= KEILA_BARE_SIDE) return true;
    return (KEILA_BARE[iz * (KEILA_BARE_SIDE / 8) + ix / 8] >> (ix % 8)) & 1;
}

static void insertUnique(SpatialGrid& grid, float x, float z, int index) {
    std::vector<int>& bucket = grid.buckets[grid.cellIndex(x, z)];
    if (bucket.empty() || bucket.back() != index) bucket.push_back(index);
}

static bool insidePolygon(const KeilaBuilding& b, float x, float z) {
    const float* p = KEILA_BUILDING_XZ + (size_t)b.first * 2;
    bool hit = false;
    for (int i = 0, j = b.count - 1; i < b.count; j = i++) {
        float ax = p[i * 2], az = p[i * 2 + 1], bx = p[j * 2], bz = p[j * 2 + 1];
        if ((az > z) != (bz > z) && x < (bx - ax) * (z - az) / (bz - az) + ax) hit = !hit;
    }
    return hit;
}

void clearKeilaColliders() {
    gKeilaWalls.clear();
    gKeilaBase.clear();
    gKeilaTop.clear();
    gKeilaWallGrid.clear();
    gKeilaBuildingGrid.clear();
}

void buildKeilaColliders() {
    clearKeilaColliders();
    gKeilaWallGrid.init(KEILA_WORLD_HALF, GRID_CELLS, 0.0f, 1.0f);
    gKeilaBuildingGrid.init(KEILA_WORLD_HALF, GRID_CELLS, 0.0f, 1.0f);
    gKeilaBase.resize(KEILA_BUILDING_COUNT);
    gKeilaTop.resize(KEILA_BUILDING_COUNT);
    for (int bi = 0; bi < KEILA_BUILDING_COUNT; bi++) {
        const KeilaBuilding& b = KEILA_BUILDINGS[bi];
        const float* p = KEILA_BUILDING_XZ + (size_t)b.first * 2;
        float low = 1e9f, sum = 0.0f;
        float minX = 1e9f, maxX = -1e9f, minZ = 1e9f, maxZ = -1e9f;
        for (int i = 0; i < b.count; i++) {
            float h = keilaHeight(p[i * 2], p[i * 2 + 1]);
            low = fminf(low, h);
            sum += h;
            minX = fminf(minX, p[i * 2]);     maxX = fmaxf(maxX, p[i * 2]);
            minZ = fminf(minZ, p[i * 2 + 1]); maxZ = fmaxf(maxZ, p[i * 2 + 1]);
        }
        gKeilaBase[bi] = low - 0.5f;
        gKeilaTop[bi]  = sum / (float)b.count + b.height;
        const float cell = gKeilaBuildingGrid.cellSz;
        for (float z = minZ; ; z = fminf(z + cell, maxZ)) {
            for (float x = minX; ; x = fminf(x + cell, maxX)) {
                insertUnique(gKeilaBuildingGrid, x, z, bi);
                if (x >= maxX) break;
            }
            if (z >= maxZ) break;
        }
        for (int i = 0; i < b.count; i++) {
            int j = (i + 1) % b.count;
            KeilaWall w;
            w.ax = p[i * 2]; w.az = p[i * 2 + 1];
            w.bx = p[j * 2]; w.bz = p[j * 2 + 1];
            float dx = w.bx - w.ax, dz = w.bz - w.az;
            float len = sqrtf(dx * dx + dz * dz);
            if (len < 1e-4f) continue;
            w.nx = dz / len; w.nz = -dx / len;
            w.bottom = gKeilaBase[bi];
            w.top    = gKeilaTop[bi];
            int index = (int)gKeilaWalls.size();
            gKeilaWalls.push_back(w);
            int steps = (int)ceilf(len / WALL_SAMPLE);
            for (int s = 0; s <= steps; s++) {
                float t = (float)s / (float)steps;
                insertUnique(gKeilaWallGrid, w.ax + dx * t, w.az + dz * t, index);
            }
        }
    }
}

float keilaBuildingBase(int building) { return gKeilaBase[building]; }
float keilaBuildingTop(int building)  { return gKeilaTop[building]; }

float keilaRoofUnder(float x, float z, float fromY, float floor) {
    gKeilaBuildingGrid.forEachNear(x, z, 0.1f, [&](int bi) {
        float top = gKeilaTop[bi];
        if (top <= floor || fromY < top - 0.05f) return;
        if (insidePolygon(KEILA_BUILDINGS[bi], x, z)) floor = top;
    });
    return floor;
}

void keilaCollidePlayer(glm::vec3& pos, float radius, float bodyHeight) {
    for (int pass = 0; pass < 2; pass++) {
        gKeilaWallGrid.forEachNear(pos.x, pos.z, radius + QUERY_PAD, [&](int wi) {
            const KeilaWall& w = gKeilaWalls[wi];
            if (pos.y >= w.top - 0.05f || w.bottom > pos.y + bodyHeight) return;
            float ex = w.bx - w.ax, ez = w.bz - w.az;
            float t = ((pos.x - w.ax) * ex + (pos.z - w.az) * ez) / (ex * ex + ez * ez);
            float tc = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            float cx = w.ax + ex * tc, cz = w.az + ez * tc;
            float dx = pos.x - cx, dz = pos.z - cz;
            float d2 = dx * dx + dz * dz;
            if (d2 >= radius * radius) return;
            float side = dx * w.nx + dz * w.nz;
            if (t > 0.0f && t < 1.0f) {
                float push = radius - side;
                pos.x += w.nx * push;
                pos.z += w.nz * push;
            } else if (d2 > 1e-8f) {
                float d = sqrtf(d2), push = radius - d;
                pos.x += dx / d * push;
                pos.z += dz / d * push;
            }
        });
    }
}

bool keilaSweep(const glm::vec3& start, const glm::vec3& end, float& fraction, glm::vec3& normal) {
    if (gKeilaWallGrid.empty()) return false;
    glm::vec3 d = end - start;
    float reach = 0.5f * sqrtf(d.x * d.x + d.z * d.z) + QUERY_PAD;
    float mx = 0.5f * (start.x + end.x), mz = 0.5f * (start.z + end.z);
    float best = 2.0f;
    gKeilaWallGrid.forEachNear(mx, mz, reach, [&](int wi) {
        const KeilaWall& w = gKeilaWalls[wi];
        float ex = w.bx - w.ax, ez = w.bz - w.az;
        float denom = d.x * ez - d.z * ex;
        if (fabsf(denom) < 1e-9f) return;
        float ox = w.ax - start.x, oz = w.az - start.z;
        float t = (ox * ez - oz * ex) / denom;
        float s = (ox * d.z - oz * d.x) / denom;
        if (t < 0.0f || t > 1.0f || s < 0.0f || s > 1.0f || t >= best) return;
        float y = start.y + d.y * t;
        if (y < w.bottom || y > w.top) return;
        best = t;
        float facing = d.x * w.nx + d.z * w.nz;
        normal = facing > 0.0f ? glm::vec3(-w.nx, 0.0f, -w.nz) : glm::vec3(w.nx, 0.0f, w.nz);
    });
    if (fabsf(d.y) > 1e-6f) {
        gKeilaBuildingGrid.forEachNear(mx, mz, reach, [&](int bi) {
            float t = (gKeilaTop[bi] - start.y) / d.y;
            if (t < 0.0f || t > 1.0f || t >= best) return;
            if (!insidePolygon(KEILA_BUILDINGS[bi], start.x + d.x * t, start.z + d.z * t)) return;
            best = t;
            normal = glm::vec3(0.0f, d.y < 0.0f ? 1.0f : -1.0f, 0.0f);
        });
    }
    if (best > 1.0f) return false;
    fraction = best;
    return true;
}
