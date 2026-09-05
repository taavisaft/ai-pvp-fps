#include "tree_collision.h"
#include <cmath>

bool segmentTreeTrunk(const TreeInstance& tree, const glm::vec3& start,
                      const glm::vec3& end, float& fraction, glm::vec3& normal) {
    if (tree.scale <= 0.0f) return false;
    const float radius = TREE_TRUNK_BASE * tree.scale;
    // Cheap conservative rejection before transforming into the six-sided prism.
    if (fminf(start.x, end.x) > tree.x + radius ||
        fmaxf(start.x, end.x) < tree.x - radius ||
        fminf(start.z, end.z) > tree.z + radius ||
        fmaxf(start.z, end.z) < tree.z - radius ||
        fminf(start.y, end.y) > tree.y + TREE_TRUNK_HEIGHT * tree.scale ||
        fmaxf(start.y, end.y) < tree.y) return false;
    // veg.vert rotates x/z by +yaw (legacy instance yaw is in radians).
    const float c = cosf(tree.yaw), s = sinf(tree.yaw);
    auto local = [&](glm::vec3 p) {
        p = (p - glm::vec3(tree.x, tree.y, tree.z)) / tree.scale;
        return glm::vec3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
    };
    const glm::vec3 p = local(start), d = local(end) - p;
    float enter = 0.0f, leave = 1.0f;
    glm::vec3 entryNormal(0, 1, 0);
    auto clip = [&](glm::vec3 n, float limit) {
        const float distance = glm::dot(n, p) - limit;
        const float speed = glm::dot(n, d);
        if (fabsf(speed) < 1e-10f) return distance <= 0.0f;
        const float t = -distance / speed;
        if (speed < 0.0f) {
            if (t >= enter) { enter = t; entryNormal = n; }
        } else leave = fminf(leave, t);
        return enter <= leave;
    };
    if (!clip({0, -1, 0}, 0) || !clip({0, 1, 0}, TREE_TRUNK_HEIGHT)) return false;
    const float apothem = cosf(3.1415926536f / TREE_TRUNK_SIDES);
    const float taper = (TREE_TRUNK_BASE - TREE_TRUNK_TOP) * apothem / TREE_TRUNK_HEIGHT;
    for (int i = 0; i < TREE_TRUNK_SIDES; ++i) {
        const float angle = (i + 0.5f) * 6.2831853f / TREE_TRUNK_SIDES;
        if (!clip({cosf(angle), taper, sinf(angle)}, TREE_TRUNK_BASE * apothem)) return false;
    }
    fraction = enter;
    normal = glm::normalize(glm::vec3(c * entryNormal.x - s * entryNormal.z,
                                     entryNormal.y, s * entryNormal.x + c * entryNormal.z));
    return true;
}

bool sweepTreeTrunks(const glm::vec3& start, const glm::vec3& end,
                     float& fraction, glm::vec3& normal) {
    const glm::vec3 mid = (start + end) * 0.5f;
    const float extent = fmaxf(fabsf(end.x - start.x), fabsf(end.z - start.z)) * 0.5f;
    float best = 2.0f;
    gTreeColGrid.forEachNear(mid.x, mid.z, extent + gTreeTrunkMaxRadius, [&](int i) {
        float t;
        glm::vec3 n;
        if (segmentTreeTrunk(gTrees[i], start, end, t, n) && t < best) {
            best = t; normal = n;
        }
    });
    if (best > 1.0f) return false;
    fraction = best;
    return true;
}
