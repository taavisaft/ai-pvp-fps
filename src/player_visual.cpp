#include "player_visual.h"
#include "weapon_visual.h"
#include "playerpose.h"
#include <glm/gtc/matrix_transform.hpp>


// Draws the held weapon anchored to the camera and
// lerped between hipfire (lower-right) and ADS (centered under crosshair).
void drawViewModel(Renderer& r, const Camera& cam, const ViewModel& vm, uint8_t weaponId) {
    glm::vec3 front = cam.front();
    glm::vec3 up    = cam.up();                                   // rolled by lean
    glm::vec3 right = glm::normalize(glm::cross(front, up));

    glm::vec3 hipOff = right * 0.17f - up * 0.20f + front * 0.48f;
    // Uzi iron sight aperture and front post share local y=0.080.
    glm::vec3 adsOff = (weaponId == WEP_UZI)
        ? (-up * 0.080f + front * 0.32f)
        : (-up * 0.05f + front * 0.30f);
    glm::vec3 off    = glm::mix(hipOff, adsOff, vm.adsT);
    glm::vec3 anchor = cam.eyePos() + off - front * (vm.recoilT * 0.08f);

    glm::mat4 basis(glm::vec4(right, 0), glm::vec4(up, 0),
                    glm::vec4(front, 0), glm::vec4(0, 0, 0, 1));
    glm::mat4 anchorM = glm::translate(glm::mat4(1.0f), anchor) * basis;
    // Camera right/up/forward reflects the authored +Z-forward mesh basis.
    // Reverse winding for this pass, then restore before any world/HUD drawing.
    glFrontFace(GL_CW);
    auto part = [&](glm::vec3 lp, glm::vec3 sz, glm::vec3 col) {
        glm::mat4 m = glm::scale(glm::translate(anchorM, lp), sz);
        r.drawCubeModel(m, col);
    };
    const glm::vec3 metal = {0.12f, 0.12f, 0.14f};
    const glm::vec3 dark  = {0.20f, 0.20f, 0.23f};
    const glm::vec3 poly  = {0.08f, 0.08f, 0.09f};  // pistol polymer frame

    if (weaponId == WEP_GLOCK19) {                 // compact pistol
        part({0.0f,  0.02f,  0.06f}, {0.050f, 0.060f, 0.20f}, metal); // slide
        part({0.0f, -0.03f,  0.03f}, {0.045f, 0.050f, 0.15f}, poly);  // frame
        part({0.0f,  0.02f,  0.17f}, {0.026f, 0.026f, 0.05f}, dark);  // barrel tip
        part({0.0f, -0.12f, -0.05f}, {0.050f, 0.130f, 0.06f}, poly);  // grip
        part({0.0f, -0.18f, -0.05f}, {0.046f, 0.040f, 0.05f}, metal); // mag base
    } else {
        r.drawMeshModel(r.uzi, anchorM, glm::vec3(1.0f));
    }

    // Grip + muzzle come from the shared per-weapon anchor table (weapon_visual.h), so
    // every gun's hand placement and barrel length live in one place.
    const WeaponVisual& wv = weaponVisual(weaponId);
    const float gripY = wv.fpFireGrip.y, gripZ = wv.fpFireGrip.z;

    // Right (firing) hand only — PUBG-style, minimal screen space. A fist wrapping
    // the grip plus a short forearm angled down-back so it leaves frame fast; the gun
    // sits in front and occludes most of it. ADS/recoil come free from anchor space.
    const glm::vec3 glove = {0.46f, 0.35f, 0.28f};   // tan fingerless glove
    glm::vec3 fistC = {wv.fpFireGrip.x, gripY, gripZ};  // fist centered on the grip
    r.drawMeshModel(r.playerPart[Renderer::PART_HAND], glm::scale(glm::translate(anchorM, fistC),
                               glm::vec3(0.085f, 0.095f, 0.105f)), glove);
    // Forearm: pivot at the wrist (just under the fist), tilt back about the right
    // axis so the limb runs from the lower-right toward the shoulder, then hang it.
    glm::mat4 fore = glm::translate(anchorM, glm::vec3(0.02f, gripY - 0.05f, gripZ - 0.01f))
                   * glm::rotate(glm::mat4(1.0f), glm::radians(24.0f), glm::vec3(1, 0, 0))
                   * glm::rotate(glm::mat4(1.0f), glm::radians(14.0f), glm::vec3(0, 0, 1))
                   * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.11f, 0.0f));
    r.drawMeshModel(r.playerPart[Renderer::PART_FOREARM],
                    glm::scale(fore, glm::vec3(0.072f, 0.22f, 0.082f)), glove);
    glFrontFace(GL_CCW);

    if (vm.flashTimer > 0.0f) {                                    // muzzle flash
        glm::vec3 muzzle = anchor + up * wv.fpMuzzle.y + front * wv.fpMuzzle.z;
        r.drawCube(muzzle, glm::vec3(0.16f), {1.0f, 0.85f, 0.35f});
    }
}


// Render authored segmented geometry using the shared gameplay pose.
void drawPlayerSkeleton(Renderer& r, const glm::vec3& pos, float yaw, float pitch,
                               float lean, uint8_t weaponId, float crouch, float ads,
                               float phase, float amp, const glm::vec3& bodyCol, bool lowDetail) {
    const glm::vec3 limb     = bodyCol * 0.85f;
    const glm::vec3 skin     = {0.90f, 0.78f, 0.62f};
    const glm::vec3 handCol  = {0.30f, 0.30f, 0.33f};
    const glm::vec3 gunMetal = {0.12f, 0.12f, 0.14f};
    const glm::vec3 gunDark  = {0.20f, 0.20f, 0.23f};

    PoseBox boxes[MAX_POSE_BOXES];
    int n = buildPlayerPose(pos, yaw, pitch, lean, weaponId, crouch, ads, phase, amp,
                            boxes);
    int armIndex = 0, legIndex = 0;
    for (int i = 0; i < n; i++) {
        if (weaponId == WEP_UZI && boxes[i].part == POSE_GUN_DARK) continue;
        if (weaponId == WEP_UZI && boxes[i].part == POSE_GUN_METAL) {
            // Recover hold from the shared weapon proxy, align its firing grip.
            glm::mat4 hold = glm::translate(boxes[i].M, glm::vec3(0, -.02f, -.16f));
            r.drawMeshModel(lowDetail ? r.uziLod : r.uzi, glm::translate(hold, glm::vec3(0, .03f, .06f)),
                            glm::vec3(1.0f));
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

