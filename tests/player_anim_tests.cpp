#include "player_anim.h"
#include "playerpose.h"
#include "map.h"
#include <initializer_list>
#include <cstdio>

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); ++failures; } } while (0)

static bool finitePose(const PoseBox* boxes, int n) {
    for (int i = 0; i < n; ++i)
        for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r)
            if (!std::isfinite(boxes[i].M[c][r])) return false;
    return n > 0;
}

int main() {
    setMap(MAP_LOBBY);
    const float dt = 1.0f / 60.0f;
    PlayerAnimState still;
    glm::vec3 origin(0.0f, terrainHeight(0, 0), 0.0f);
    for (int i = 0; i < 180; ++i) still.update(origin, false, 0.0f, dt);
    CHECK(still.pose.amp < 0.01f && still.pose.run < 0.01f);
    CHECK(still.pose.idleT > 2.9f && still.pose.airT < 0.0f && still.pose.landT < 0.0f);

    PlayerAnimState walker;
    glm::vec3 p = origin;
    float lastPhase = 0.0f;
    for (int i = 0; i < 120; ++i) {
        p.x += MOVE_SPEED * dt; p.y = terrainHeight(p.x, p.z);
        walker.update(p, false, 0.0f, dt);
        CHECK(walker.pose.phase >= lastPhase);
        lastPhase = walker.pose.phase;
    }
    CHECK(walker.pose.amp > 0.95f && walker.pose.run < 0.05f && walker.pose.idleT == 0.0f);
    CHECK(fabsf(walker.speed - MOVE_SPEED) < 0.2f);
    float walkRate = walker.pose.phase;
    p.x += MOVE_SPEED * dt; p.y = terrainHeight(p.x, p.z);
    walker.update(p, false, 0.0f, dt);
    walkRate = walker.pose.phase - walkRate;
    CHECK(fabsf(walkRate - MOVE_SPEED * 6.2831853f / WALK_CYCLE_M * dt) < 1e-3f);

    PlayerAnimState runner;
    p = origin;
    for (int i = 0; i < 240; ++i) {
        p.x += SPRINT_SPEED * dt; p.y = terrainHeight(p.x, p.z);
        runner.update(p, false, 0.0f, dt);
    }
    CHECK(runner.pose.run > 0.95f);

    PlayerAnimState jumper;
    for (int i = 0; i < 30; ++i) jumper.update(origin, false, 0.0f, dt);
    for (int i = 0; i < 20; ++i) jumper.update(origin + glm::vec3(0, 0.3f, 0), true, 1.0f, dt);
    CHECK(jumper.pose.airT > 0.3f && jumper.pose.landT < 0.0f);
    jumper.update(origin, false, 0.0f, dt);
    CHECK(jumper.pose.airT < 0.0f && jumper.pose.landT >= 0.0f);
    for (int i = 0; i < 30; ++i) jumper.update(origin, false, 0.0f, dt);
    CHECK(jumper.pose.landT < 0.0f);

    PoseBox rest[MAX_POSE_BOXES], animated[MAX_POSE_BOXES];
    int restCount = buildPlayerPose(origin, 90.0f, 0.0f, 0.0f, WEP_UZI, 0.0f, 0.0f, 0.0f, 0.0f, rest);
    for (float crouch : {0.0f, 1.0f}) for (float phase = 0.0f; phase < 6.3f; phase += 0.7f) {
        PoseAnim a; a.phase = phase; a.amp = 1.0f; a.run = crouch > 0.5f ? 0.0f : 1.0f; a.idleT = 0.0f;
        int n = buildPlayerPose(origin, 90.0f, 0.0f, 0.0f, WEP_UZI, crouch, 0.0f, phase, 1.0f, animated, &a);
        CHECK(n == restCount && finitePose(animated, n));
        for (int i = 0; i < n; ++i) {
            if (animated[i].part != POSE_FOOT) continue;
            float footY = animated[i].M[3][1];
            CHECK(footY > origin.y - 0.05f && footY < origin.y + 0.45f);
        }
    }
    PoseAnim air; air.airT = 0.4f; air.velY = 2.0f;
    int n = buildPlayerPose(origin + glm::vec3(0, 0.5f, 0), 90.0f, 0.0f, 0.0f, WEP_UZI, 0.0f, 0.0f, 0.0f, 0.0f, animated, &air);
    CHECK(finitePose(animated, n));
    for (float pitch : {-40.0f, 0.0f, 35.0f}) for (float lean : {-1.0f, 0.0f, 1.0f}) {
        PoseAnim aim;
        int count = buildPlayerPose(origin, 123.0f, pitch, lean, WEP_UZI, 0.0f, 1.0f, 0.0f, 0.0f, animated, &aim);
        glm::vec3 eye = playerEye(origin, 123.0f, lean, 0.0f);
        float yr = glm::radians(123.0f), pr = glm::radians(pitch);
        glm::vec3 front(cosf(yr) * cosf(pr), sinf(pr), sinf(yr) * cosf(pr));
        bool found = false;
        for (int i = 0; i < count; ++i) {
            if (animated[i].part != POSE_GUN_METAL) continue;
            glm::mat4 hold = glm::translate(animated[i].M, glm::vec3(0, -.02f, -.16f));
            glm::vec3 sight = glm::vec3(hold * glm::vec4(0, 0.11f, 0.06f, 1));
            CHECK(glm::length(sight - (eye + front * 0.32f)) < 0.01f);
            found = true;
        }
        CHECK(found);
    }
    PoseBox plain[MAX_POSE_BOXES];
    int plainCount = buildPlayerPose(origin, 90.0f, 0.0f, 0.0f, WEP_UZI, 0.0f, 0.0f, 0.0f, 0.0f, plain);
    CHECK(plainCount == restCount);
    for (int i = 0; i < plainCount; ++i) CHECK(plain[i].M == rest[i].M && plain[i].half == rest[i].half);
    return failures ? 1 : 0;
}
