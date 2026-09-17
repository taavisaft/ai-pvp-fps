#include "renderer.h"
#include "renderer_bind.h"
#include "map.h"
#include "texture.h"
#include <cmath>
#include <vector>

enum FacadeCell { CELL_WINDOW_PANEL = 0, CELL_WINDOW, CELL_CLADDING, CELL_PLASTER,
                  CELL_PLINTH, CELL_PLINTH_WINDOW, CELL_DOOR, CELL_STAIR };

static void pushQuad(std::vector<float>& v, std::vector<unsigned>& ix, const glm::vec3& a,
                     const glm::vec3& b, float y0, float y1, const glm::vec3& n, int cell) {
    const float inset = 3.0f;
    float u1 = ((cell % 4) * 512.0f + inset) / 2048.0f;
    float u0 = ((cell % 4) * 512.0f + 512.0f - inset) / 2048.0f;
    float vTop = 1.0f - ((cell / 4) * 512.0f + inset) / 1024.0f;
    float vBottom = 1.0f - ((cell / 4) * 512.0f + 512.0f - inset) / 1024.0f;
    unsigned base = (unsigned)(v.size() / 8);
    const float data[32] = {
        a.x, y0, a.z, n.x, n.y, n.z, u0, vBottom,
        b.x, y0, b.z, n.x, n.y, n.z, u1, vBottom,
        b.x, y1, b.z, n.x, n.y, n.z, u1, vTop,
        a.x, y1, a.z, n.x, n.y, n.z, u0, vTop};
    v.insert(v.end(), data, data + 32);
    const unsigned quad[6] = {base, base + 2, base + 1, base, base + 3, base + 2};
    ix.insert(ix.end(), quad, quad + 6);
}

static int explicitCell(char code, int floor) {
    switch (code) {
        case 'A': return CELL_WINDOW_PANEL;
        case 'B': return CELL_WINDOW;
        case 'C': return CELL_CLADDING;
        case 'D': return floor == 0 ? CELL_DOOR : CELL_STAIR;
        case 'S': return CELL_STAIR;
        default:  return CELL_PLASTER;
    }
}

static int bayCell(char style, int bay, int bays, int floor) {
    if (style == 'C') return CELL_CLADDING;
    if (style == 'P') return CELL_PLASTER;
    if (style == 'D' && bay == bays / 2) return floor == 0 ? CELL_DOOR : CELL_STAIR;
    if (style == 'S' && bay == bays / 2) return CELL_STAIR;
    return bay % 3 == 2 ? CELL_WINDOW : CELL_WINDOW_PANEL;
}

void Renderer::buildKeilaLandmarks() {
    for (int li = 0; li < KEILA_LANDMARK_COUNT && li < KEILA_MAX_LANDMARKS; li++) {
        const KeilaLandmark& lm = KEILA_LANDMARKS[li];
        const KeilaBuilding& b = KEILA_BUILDINGS[lm.building];
        const float* p = KEILA_BUILDING_XZ + (size_t)b.first * 2;
        float bottom = keilaBuildingBase(lm.building), top = keilaBuildingTop(lm.building);
        float ground = top - b.height;
        float plinthTop = ground + lm.plinth;
        float floorHeight = (top - lm.parapet - plinthTop) / (float)lm.floors;
        std::vector<float> v;
        std::vector<unsigned> ix;
        keilaLandmarkMin[li] = glm::vec3(1e9f);
        keilaLandmarkMax[li] = glm::vec3(-1e9f);
        const char* spec = lm.walls;
        for (int i = 0; i < b.count; i++) {
            const char* wallSpec = spec;
            int specLen = 0;
            while (spec[specLen] && spec[specLen] != '|') specLen++;
            spec += specLen + (spec[specLen] == '|' ? 1 : 0);
            int j = (i + 1) % b.count;
            glm::vec3 a(p[i * 2], 0.0f, p[i * 2 + 1]), c(p[j * 2], 0.0f, p[j * 2 + 1]);
            glm::vec3 d = c - a;
            float len = glm::length(d);
            if (len < 1e-4f) continue;
            glm::vec3 n(d.z / len, 0.0f, -d.x / len);
            char style = specLen > 0 ? wallSpec[0] : 'P';
            bool manual = specLen > 1;
            int bays = manual ? specLen : (int)fmaxf(1.0f, roundf(len / 3.0f));
            for (int k = 0; k < bays; k++) {
                glm::vec3 s = a + d * ((float)k / bays), e = a + d * ((float)(k + 1) / bays);
                char code = manual ? wallSpec[bays - 1 - k] : style;
                bool plain = code == 'C' || code == 'P' || code == 'D';
                pushQuad(v, ix, s, e, bottom, plinthTop, n,
                         plain || k % 2 ? CELL_PLINTH : CELL_PLINTH_WINDOW);
                for (int f = 0; f < lm.floors; f++)
                    pushQuad(v, ix, s, e, plinthTop + floorHeight * f, plinthTop + floorHeight * (f + 1),
                             n, manual ? explicitCell(code, f) : bayCell(style, k, bays, f));
                pushQuad(v, ix, s, e, top - lm.parapet, top, n,
                         style == 'C' ? CELL_CLADDING : CELL_PLASTER);
            }
            keilaLandmarkMin[li] = glm::min(keilaLandmarkMin[li], glm::vec3(a.x, bottom, a.z));
            keilaLandmarkMax[li] = glm::max(keilaLandmarkMax[li], glm::vec3(a.x, top, a.z));
        }
        keilaLandmark[li].create(v.data(), v.size(), ix.data(), ix.size(), true, true, false);
        keilaLandmarkTex[li] = loadTexture(lm.texture);
        if (keilaLandmarkTex[li]) {
            glBindTexture(GL_TEXTURE_2D, keilaLandmarkTex[li]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 5);
        }
    }
}

void Renderer::drawKeilaLandmarks(const Frustum& fr) {
    for (int li = 0; li < KEILA_LANDMARK_COUNT && li < KEILA_MAX_LANDMARKS; li++) {
        if (!keilaLandmark[li].indexCount || !keilaLandmarkTex[li]) continue;
        glm::vec3 center = (keilaLandmarkMin[li] + keilaLandmarkMax[li]) * 0.5f;
        glm::vec3 half   = (keilaLandmarkMax[li] - keilaLandmarkMin[li]) * 0.5f;
        if (!fr.aabbVisible(center, half)) continue;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, keilaLandmarkTex[li]);
        active->setInt(active->locUseTex, 1);
        active->setInt(active->locUseFacade, 1);
        active->setInt(active->locHasNormal, 1);
        active->setVec3(active->locTint, glm::vec3(1.0f));
        active->setFloat(active->locSpec, 0.0f);
        active->setMat4(active->locModel, glm::mat4(1.0f));
        active->setVec3(active->locColor, glm::vec3(1.0f));
        keilaLandmark[li].draw();
        active->setInt(active->locHasNormal, 0);
        active->setInt(active->locUseFacade, 0);
    }
}
