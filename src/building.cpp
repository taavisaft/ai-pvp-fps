#include "building.h"
#include "texture.h"   // uploadTextureRGB
#include <vector>
#include <cmath>

// --- procedural facade cell ------------------------------------------------
// One panel = one window with a precast-concrete border. The cell tiles seamlessly
// (seam grooves sit on the cell edges so neighbours share a joint line).
namespace {
float fhash(float x, float y) {
    float h = sinf(x * 127.1f + y * 311.7f) * 43758.5453f;
    return h - floorf(h);
}
struct RGB { float r, g, b; };
RGB lerp(RGB a, RGB b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}
}  // namespace

GLuint makeFacadeTexture() {
    const int S = 256;
    std::vector<unsigned char> px((size_t)S * S * 3);

    const RGB concrete = {0.72f, 0.71f, 0.67f};
    const RGB seam     = {0.46f, 0.45f, 0.42f};
    const RGB frame    = {0.86f, 0.85f, 0.82f};
    const RGB glassTop = {0.26f, 0.31f, 0.36f};
    const RGB glassBot = {0.44f, 0.51f, 0.57f};
    const RGB mullion  = {0.82f, 0.81f, 0.78f};
    const RGB sill     = {0.76f, 0.75f, 0.70f};

    // Window box in cell-normalized coords (v=0 at bottom). Large opening + clear
    // horizontal floor band (the precast spandrel) below it.
    const float Wx0 = 0.15f, Wx1 = 0.85f, Wy0 = 0.30f, Wy1 = 0.88f;
    const float FT = 0.035f;   // frame thickness

    for (int y = 0; y < S; y++)
    for (int x = 0; x < S; x++) {
        float u = (x + 0.5f) / S;
        float v = 1.0f - (y + 0.5f) / S;   // flip so v=0 is the cell bottom

        // base concrete with fine grain
        float grain = (fhash(x * 0.7f, y * 0.7f) - 0.5f) * 0.08f;
        RGB c = {concrete.r + grain, concrete.g + grain, concrete.b + grain};

        // panel joint grooves on the cell edges
        if (u < 0.025f || u > 0.975f || v < 0.025f || v > 0.975f) c = seam;

        bool inWin = u > Wx0 && u < Wx1 && v > Wy0 && v < Wy1;
        if (inWin) {
            float fu0 = Wx0 + FT, fu1 = Wx1 - FT, fv0 = Wy0 + FT, fv1 = Wy1 - FT;
            bool glass = u > fu0 && u < fu1 && v > fv0 && v < fv1;
            if (!glass) {
                c = frame;                                   // window frame
            } else {
                float gv = (v - fv0) / (fv1 - fv0);
                c = lerp(glassBot, glassTop, gv);            // glazing gradient
                c.r += (fhash(x, y) - 0.5f) * 0.05f;         // faint reflections
                // vertical mullions -> the reference's slatted windows (3 lights)
                float mw = 0.018f;
                if (fabsf(u - 0.40f) < mw || fabsf(u - 0.60f) < mw) c = mullion;
                if (fabsf(v - 0.60f) < mw) c = mullion;      // transom bar
            }
        } else if (v < Wy0 && u > Wx0 && u < Wx1) {
            // sill + grime streaks running down from the window
            float streak = fhash(floorf(u * 40.0f), 3.0f);
            float run = (Wy0 - v) * 1.6f;
            float dirt = streak > 0.6f ? run * 0.10f : 0.0f;
            c = sill;
            c.r -= dirt; c.g -= dirt; c.b -= dirt;
        }

        auto b = [](float f) {
            int i = (int)(f * 255.0f + 0.5f);
            return (unsigned char)(i < 0 ? 0 : i > 255 ? 255 : i);
        };
        size_t o = ((size_t)y * S + x) * 3;
        px[o] = b(c.r); px[o + 1] = b(c.g); px[o + 2] = b(c.b);
    }
    return uploadTextureRGB(px.data(), S, S);
}

// --- facade wall mesh ------------------------------------------------------
namespace {
// Append a quad given its 4 outside corners (BL, BR, TR, TL) + normal + UV extents.
void addWall(std::vector<float>& vb, std::vector<unsigned>& ib,
             glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3,
             glm::vec3 n, float uMax, float vMax) {
    unsigned base = (unsigned)(vb.size() / 8);
    glm::vec3 ps[4] = {p0, p1, p2, p3};
    glm::vec2 uv[4] = {{0, 0}, {uMax, 0}, {uMax, vMax}, {0, vMax}};
    for (int i = 0; i < 4; i++) {
        vb.insert(vb.end(), {ps[i].x, ps[i].y, ps[i].z, n.x, n.y, n.z, uv[i].x, uv[i].y});
    }
    ib.insert(ib.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}
}  // namespace

bool buildFacadeMesh(Mesh& out, const TownBuilding& b) {
    float hx = b.w * 0.5f, hz = b.d * 0.5f, h = b.h;
    float ux = (float)b.baysX, uz = (float)b.baysZ, vf = (float)b.floors;
    std::vector<float>    vb;
    std::vector<unsigned> ib;
    // +Z (north)
    addWall(vb, ib, {-hx, 0, hz}, {hx, 0, hz}, {hx, h, hz}, {-hx, h, hz}, {0, 0, 1}, ux, vf);
    // -Z (south)
    addWall(vb, ib, {hx, 0, -hz}, {-hx, 0, -hz}, {-hx, h, -hz}, {hx, h, -hz}, {0, 0, -1}, ux, vf);
    // +X (east)
    addWall(vb, ib, {hx, 0, hz}, {hx, 0, -hz}, {hx, h, -hz}, {hx, h, hz}, {1, 0, 0}, uz, vf);
    // -X (west)
    addWall(vb, ib, {-hx, 0, -hz}, {-hx, 0, hz}, {-hx, h, hz}, {-hx, h, -hz}, {-1, 0, 0}, uz, vf);
    return out.create(vb.data(), vb.size(), ib.data(), ib.size(), true, true);
}

bool buildBuildingDecalMesh(Mesh& out, const TownBuilding& b, int buildingIndex) {
    std::vector<float> vb;
    std::vector<unsigned> ib;
    auto decal = [&](glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3,
                     glm::vec3 n, int tile) {
        // Half-texel-ish inset within each 256px atlas tile limits mip bleeding.
        float col = (float)(tile & 3), row = (float)(tile >> 2);
        float e = 0.004f;
        float u0 = col * 0.25f + e, u1 = (col + 1.0f) * 0.25f - e;
        // Source images are documented top-down; stb flips the full atlas on load.
        float v0 = (3.0f - row) * 0.25f + e, v1 = (4.0f - row) * 0.25f - e;
        unsigned base = (unsigned)(vb.size() / 8);
        glm::vec3 ps[4] = {p0, p1, p2, p3};
        glm::vec2 uv[4] = {{u0,v0},{u1,v0},{u1,v1},{u0,v1}};
        for (int i = 0; i < 4; i++)
            vb.insert(vb.end(), {ps[i].x,ps[i].y,ps[i].z,n.x,n.y,n.z,uv[i].x,uv[i].y});
        ib.insert(ib.end(), {base,base+1,base+2,base,base+2,base+3});
    };
    float hx = b.w * 0.5f, hz = b.d * 0.5f;
    float y0 = 0.35f, y1 = glm::min(b.h - 0.4f, 3.8f + (buildingIndex % 3));
    float w = glm::min(3.8f, b.w * 0.28f), x = -b.w * 0.22f + (buildingIndex % 5) * 0.17f;
    decal({x-w,y0,hz+0.012f},{x+w,y0,hz+0.012f},{x+w,y1,hz+0.012f},{x-w,y1,hz+0.012f},
          {0,0,1}, buildingIndex % 6); // stains, soot, runoff, moss, algae
    float z = -b.d * 0.18f, sy = glm::min(b.h - 0.5f, 2.8f);
    decal({hx+0.012f,0.3f,z+2.0f},{hx+0.012f,0.3f,z-2.0f},
          {hx+0.012f,sy,z-2.0f},{hx+0.012f,sy,z+2.0f},{1,0,0},
          8 + (buildingIndex % 4)); // cracks, rust, peeling paint
    return out.create(vb.data(), vb.size(), ib.data(), ib.size(), true, true);
}
