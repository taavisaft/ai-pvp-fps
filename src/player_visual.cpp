#include "player_visual.h"
#include "weapon_visual.h"
#include "playerpose.h"
#include <glm/gtc/matrix_transform.hpp>

// Lightweight shared rifle silhouette for first- and third-person views.
static void drawKar98(Renderer& r, const glm::mat4& mount) {
    r.drawMeshModel(r.kar98, mount, glm::vec3(1.0f));
}


glm::mat4 playerWeaponHold(const Player& player, float crouch, float ads, const PoseAnim& anim) {
    PoseBox boxes[MAX_POSE_BOXES];
    int n = buildPlayerPose(player.pos, player.yaw, player.pitch, player.lean, player.weaponId,
                            crouch, ads, anim.phase, anim.amp, boxes, &anim);
    for (int i = 0; i < n; ++i)
        if (boxes[i].part == POSE_GUN_METAL)
            return glm::translate(boxes[i].M, glm::vec3(0, -.02f, -.16f));
    return glm::mat4(1.0f);
}

// Render authored segmented geometry using the shared gameplay pose.
void drawPlayerSkeleton(Renderer& r, const glm::vec3& pos, float yaw, float pitch,
                               float lean, uint8_t weaponId, float crouch, float ads,
                               const PoseAnim& anim, const glm::vec3& bodyCol, bool lowDetail, float uziBolt,
                               bool firstPerson) {
    const glm::vec3 limb     = bodyCol * 0.85f;
    const glm::vec3 skin     = {0.90f, 0.78f, 0.62f};
    const glm::vec3 handCol  = {0.30f, 0.30f, 0.33f};
    const glm::vec3 gunMetal = {0.12f, 0.12f, 0.14f};
    const glm::vec3 gunDark  = {0.20f, 0.20f, 0.23f};

    PoseBox boxes[MAX_POSE_BOXES];
    int n = buildPlayerPose(pos, yaw, pitch, lean, weaponId, crouch, ads, anim.phase, anim.amp,
                            boxes, &anim);
    int armIndex = 0, legIndex = 0;
    const bool scoped = firstPerson && weaponId == WEP_KAR98 && ads > 0.85f;
    for (int i = 0; i < n; i++) {
        if (boxes[i].part == POSE_GUN_DARK) continue;
        if (firstPerson && (boxes[i].part == POSE_HEAD || boxes[i].part == POSE_NECK || boxes[i].part == POSE_NOSE)) continue;
        if (boxes[i].part == POSE_GUN_METAL && anim.flash > 0.0f) {
            glm::mat4 hold = glm::translate(boxes[i].M, glm::vec3(0, -.02f, -.16f));
            const WeaponVisual& wv = weaponVisual(weaponId);
            glm::vec3 muzzle = weaponId == WEP_UZI && useImportedUzi()
                ? glm::vec3(hold * glm::vec4(0, .072f, .323f, 1))
                : glm::vec3(hold * glm::vec4(wv.fpMuzzle + glm::vec3(0, .03f, .06f), 1));
            r.drawCube(muzzle, glm::vec3(0.16f), {1.0f, 0.85f, 0.35f});
        }
        if (scoped && (boxes[i].part == POSE_GUN_METAL || boxes[i].part == POSE_HAND || boxes[i].part == POSE_ARM)) continue;
        if (weaponId == WEP_KAR98 && boxes[i].part == POSE_GUN_METAL) {
            glm::mat4 hold = glm::translate(boxes[i].M, glm::vec3(0, -.02f, -.16f));
            drawKar98(r, hold);
            continue;
        }
        if (weaponId == WEP_GLOCK19 && boxes[i].part == POSE_GUN_METAL) {
            glm::mat4 hold = glm::translate(boxes[i].M, glm::vec3(0, -.02f, -.16f));
            glm::mat4 anchor = glm::translate(hold, glm::vec3(0, .02f, -.04f));
            auto part = [&](glm::vec3 lp, glm::vec3 sz, glm::vec3 col) {
                r.drawCubeModel(glm::scale(glm::translate(anchor, lp), sz), col);
            };
            const glm::vec3 poly = {0.08f, 0.08f, 0.09f};
            part({0.0f,  0.02f,  0.06f}, {0.050f, 0.060f, 0.20f}, gunMetal);
            part({0.0f, -0.03f,  0.03f}, {0.045f, 0.050f, 0.15f}, poly);
            part({0.0f,  0.02f,  0.17f}, {0.026f, 0.026f, 0.05f}, gunDark);
            part({0.0f, -0.12f, -0.05f}, {0.050f, 0.130f, 0.06f}, poly);
            part({0.0f, -0.18f, -0.05f}, {0.046f, 0.040f, 0.05f}, gunMetal);
            continue;
        }
        if (weaponId == WEP_UZI && boxes[i].part == POSE_GUN_METAL) {
            // Recover hold from the shared weapon proxy, align its firing grip.
            glm::mat4 hold = glm::translate(boxes[i].M, glm::vec3(0, -.02f, -.16f));
            glm::mat4 mount=glm::translate(hold,glm::vec3(0,.03f,.06f));
            if(useImportedUzi()) r.importedUzi.draw(r,mount,uziBolt,lowDetail);
            else r.drawMeshModel(lowDetail ? r.uziLod : r.uzi,mount,glm::vec3(1));
            continue;
        }
        glm::vec3 c;
        int part = -1;   // Blender part per pose box; Glock retains its proxy
        switch (boxes[i].part) {
            case POSE_HEAD:      c = skin;    part = Renderer::PART_HEAD;   break;
            case POSE_NOSE:      continue;    // nose is baked into the head mesh
            case POSE_ARM:       c = limb;    part = (armIndex++ % 2) ? Renderer::PART_FOREARM : Renderer::PART_ARM; break;
            case POSE_LEG:       c = limb;    part = (legIndex++ % 2) ? Renderer::PART_SHIN : Renderer::PART_LEG; break;
            case POSE_FOOT:      c = limb * 0.5f; part = Renderer::PART_FOOT; break;
            case POSE_HAND:      c = handCol; part = Renderer::PART_HAND;   break;
            case POSE_PELVIS:    c = bodyCol; part = Renderer::PART_PELVIS; break;
            case POSE_TORSO:     c = bodyCol; part = Renderer::PART_TORSO;  break;
            case POSE_NECK:      c = skin;    part = Renderer::PART_NECK;   break;
            case POSE_GUN_METAL: c = gunMetal; break;
            case POSE_GUN_DARK:  c = gunDark;  break;
            default:             c = bodyCol;  break;
        }
        glm::mat4 m = boxes[i].M * glm::scale(glm::mat4(1.0f), boxes[i].half * 2.0f);
        // Shared OBB segment frames may be reflected. Cosmetic meshes use an
        // outward, right-handed frame; symmetric hit volumes remain unchanged.
        if (part == Renderer::PART_ARM || part == Renderer::PART_FOREARM ||
            part == Renderer::PART_LEG || part == Renderer::PART_SHIN ||
            part == Renderer::PART_FOOT) m[2] = -m[2];
        if (part >= 0) r.drawMeshModel(lowDetail ? r.playerLod[part] : r.playerPart[part], m, c);
        else           r.drawCubeModel(m, c);
    }
}
