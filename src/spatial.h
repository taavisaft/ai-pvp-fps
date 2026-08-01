#pragma once
#include <vector>
#include <cmath>
#include <glm/glm.hpp>
#include "frustum.h"

// Uniform grid over the XZ plane for bulk frustum culling. Static objects (city
// buildings, scattered props) register their index into the cell covering their
// footprint; per frame each cell's column AABB is frustum-tested once and only
// visible cells' contents are iterated. One test rejects a whole block of objects.
//
// Built once when a map is generated; cleared on map change. Y is not subdivided —
// cells are full-height columns spanning [yMin, yMax].
struct SpatialGrid {
    float    half   = 0.0f;   // world extends [-half, +half] on X and Z
    int      cells  = 1;      // cells per axis (grid is cells x cells)
    float    cellSz = 0.0f;   // world meters per cell
    float    yMin   = 0.0f;
    float    yMax   = 0.0f;
    std::vector<std::vector<int>> buckets;  // size cells*cells, object indices

    void init(float worldHalf, int cellsPerAxis, float colYMin, float colYMax) {
        half   = worldHalf;
        cells  = cellsPerAxis < 1 ? 1 : cellsPerAxis;
        cellSz = (2.0f * half) / cells;
        yMin   = colYMin;
        yMax   = colYMax;
        buckets.assign((size_t)cells * cells, {});
    }

    int cellIndex(float x, float z) const {
        int cx = (int)((x + half) / cellSz);
        int cz = (int)((z + half) / cellSz);
        cx = cx < 0 ? 0 : (cx >= cells ? cells - 1 : cx);
        cz = cz < 0 ? 0 : (cz >= cells ? cells - 1 : cz);
        return cz * cells + cx;
    }

    void insert(float x, float z, int objIndex) {
        if (buckets.empty()) return;
        buckets[cellIndex(x, z)].push_back(objIndex);
    }

    void clear() { buckets.clear(); }
    bool empty() const { return buckets.empty(); }

    // Call fn(objIndex) for every object in a cell whose column AABB is visible.
    // yMax may be raised per cell elsewhere; here we use the grid-wide column.
    template <typename Fn>
    void forEachVisible(const Frustum& fr, Fn&& fn) const {
        float cy   = (yMin + yMax) * 0.5f;
        float hy   = (yMax - yMin) * 0.5f;
        glm::vec3 half3(cellSz * 0.5f, hy, cellSz * 0.5f);
        for (int cz = 0; cz < cells; cz++)
        for (int cx = 0; cx < cells; cx++) {
            const auto& b = buckets[(size_t)cz * cells + cx];
            if (b.empty()) continue;
            glm::vec3 c(-half + (cx + 0.5f) * cellSz, cy,
                       -half + (cz + 0.5f) * cellSz);
            if (!fr.aabbVisible(c, half3)) continue;
            for (int idx : b) fn(idx);
        }
    }

    // Call fn(objIndex) for every object in the cells overlapping the XZ square of
    // half-width `rad` around (x,z). Used for collision neighbourhood queries — the
    // caller does the exact circle test. A few cells, no per-object distance here.
    template <typename Fn>
    void forEachNear(float x, float z, float rad, Fn&& fn) const {
        if (buckets.empty() || cellSz <= 0.0f) return;
        auto clampCell = [&](float w) {
            int c = (int)((w + half) / cellSz);
            return c < 0 ? 0 : (c >= cells ? cells - 1 : c);
        };
        int cx0 = clampCell(x - rad), cx1 = clampCell(x + rad);
        int cz0 = clampCell(z - rad), cz1 = clampCell(z + rad);
        for (int cz = cz0; cz <= cz1; cz++)
        for (int cx = cx0; cx <= cx1; cx++)
            for (int idx : buckets[(size_t)cz * cells + cx]) fn(idx);
    }

    // As forEachVisible, plus an XZ distance cap: cells whose nearest edge is
    // beyond maxDist from eye are skipped before their bucket is touched. XZ
    // nearest-point is conservative (3D dist >= XZ dist), so no in-range object is
    // ever dropped — the caller's per-object distance test still does the fine cut.
    // This is the ridge win: a long frustom no longer buckets the whole map.
    template <typename Fn>
    void forEachVisibleWithin(const Frustum& fr, const glm::vec3& eye,
                              float maxDist, Fn&& fn) const {
        float cy   = (yMin + yMax) * 0.5f;
        float hy   = (yMax - yMin) * 0.5f;
        float hc   = cellSz * 0.5f;
        float md2  = maxDist * maxDist;
        glm::vec3 half3(hc, hy, hc);
        for (int cz = 0; cz < cells; cz++)
        for (int cx = 0; cx < cells; cx++) {
            const auto& b = buckets[(size_t)cz * cells + cx];
            if (b.empty()) continue;
            glm::vec3 c(-half + (cx + 0.5f) * cellSz, cy,
                       -half + (cz + 0.5f) * cellSz);
            float ndx = fmaxf(fabsf(eye.x - c.x) - hc, 0.0f);
            float ndz = fmaxf(fabsf(eye.z - c.z) - hc, 0.0f);
            if (ndx * ndx + ndz * ndz > md2) continue;
            if (!fr.aabbVisible(c, half3)) continue;
            for (int idx : b) fn(idx);
        }
    }
};
