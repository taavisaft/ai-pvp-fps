#include "vegetation.h"
#include <cmath>

static unsigned pushV(std::vector<float>& v, glm::vec3 p, glm::vec3 n, glm::vec3 c, float flex) {
    unsigned i = (unsigned)(v.size()/12);
    v.insert(v.end(), {p.x,p.y,p.z,n.x,n.y,n.z,c.x,c.y,c.z,flex,-2.0f,flex});
    return i;
}

// One grass instance = a tuft of seven tapered blades fanned around the root
// (7x the visual density per instance for the same instance-stream cost).
// Up-facing normals on purpose: blades then take exactly the ground's lighting
// and read as part of the field instead of dark spikes on it.
void vegBuildBlade(std::vector<float>& v, std::vector<unsigned>& idx) {
    const glm::vec3 root(0.065f, 0.095f, 0.025f);
    const glm::vec3 tip (0.24f, 0.30f, 0.080f);
    const glm::vec3 n(0.0f, 1.0f, 0.0f);
    struct Row { float y, w, bend, flex; };
    const Row rows[3] = {{0.00f, 0.014f, 0.00f, 0.0f},
                         {0.22f, 0.010f, 0.03f, 0.3f},
                         {0.37f, 0.006f, 0.08f, 0.65f}};
    const float bladeYaw[7]  = {0.0f, 2.19f, 4.35f, 1.1f, 3.2f, 5.4f, .5f};   // fan directions
    const float bladeSize[7] = {1.0f, .78f, .62f, .90f, .55f, .72f, .48f};
    for (int b = 0; b < 7; b++) {
        float cy = cosf(bladeYaw[b]), sy = sinf(bladeYaw[b]);
        float sc = bladeSize[b];
        glm::vec3 off(cy * 0.065f, 0.0f, sy * 0.065f);   // root offset from center
        auto place = [&](float x, float y, float z) {
            glm::vec3 p = glm::vec3(cy * x - sy * z, y, sy * x + cy * z) * sc + off;
            return p;
        };
        unsigned r[3][2];
        for (int i = 0; i < 3; i++) {
            glm::vec3 c = root + (tip - root) * (rows[i].y / 0.48f);
            r[i][0] = pushV(v, place(-rows[i].w, rows[i].y, rows[i].bend), n, c, rows[i].flex);
            r[i][1] = pushV(v, place( rows[i].w, rows[i].y, rows[i].bend), n, c, rows[i].flex);
        }
        unsigned t = pushV(v, place(0.0f, 0.48f, 0.15f), n, tip, 1.0f);
        for (int i = 0; i < 2; i++)
            idx.insert(idx.end(), {r[i][0], r[i][1], r[i + 1][1],
                                   r[i][0], r[i + 1][1], r[i + 1][0]});
        idx.insert(idx.end(), {r[2][0], r[2][1], t});
    }
}
