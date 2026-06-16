#include "camera.h"
#include "game.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

static constexpr float SENSITIVITY = 0.1f;

void Camera::addLook(float xrel, float yrel) {
    yaw   += xrel * SENSITIVITY;
    pitch -= yrel * SENSITIVITY;
    if (pitch >  89.0f) pitch =  89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}

void Camera::updateLean(float target, float dt) {
    float k = dt * LEAN_LERP_SPEED;
    if (k > 1.0f) k = 1.0f;
    lean += (target - lean) * k;
}

float Camera::aimYaw() const { return yaw + recoilYaw; }

float Camera::aimPitch() const {
    float p = pitch + recoilPitch;
    if (p >  89.0f) p =  89.0f;
    if (p < -89.0f) p = -89.0f;
    return p;
}

void Camera::applyRecoil(float dPitch, float dYaw) {
    recoilPitch += dPitch;
    recoilYaw += dYaw;
}

void Camera::recoverRecoil(float dt, bool firingRecently) {
    if (firingRecently) return;                 // let the spray climb while firing
    (void)dt;
    // Sticky recoil: when firing stops, bake the climbed offset into the real aim so
    // the view stays where the spray left it — no auto-return to the original aim.
    // The player pulls back down by hand. Sum is unchanged, so there's no visual jump.
    pitch += recoilPitch;
    yaw   += recoilYaw;
    if (pitch >  89.0f) pitch =  89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    recoilPitch = 0.0f;
    recoilYaw   = 0.0f;
}

glm::vec3 Camera::front() const {
    float y = aimYaw(), p = aimPitch();
    return glm::normalize(glm::vec3(
        cos(glm::radians(y)) * cos(glm::radians(p)),
        sin(glm::radians(p)),
        sin(glm::radians(y)) * cos(glm::radians(p))
    ));
}

glm::vec3 Camera::rightFlat() const {
    // Horizontal right of the look direction (y=0), independent of pitch.
    return glm::normalize(glm::cross(front(), glm::vec3(0, 1, 0)));
}

glm::vec3 Camera::eyePos() const {
    LeanShift s = leanShift(lean);                 // head arcs sideways + dips
    return eye + rightFlat() * s.lateral - glm::vec3(0.0f, s.drop, 0.0f);
}

glm::vec3 Camera::up() const {
    glm::vec3 f = front();
    glm::vec3 baseUp = glm::normalize(glm::cross(rightFlat(), f));
    if (lean == 0.0f) return baseUp;
    // Roll the up vector by the same body-tilt angle so the head leans with the arc.
    glm::mat4 rot = glm::rotate(glm::mat4(1.0f), lean * glm::radians(LEAN_ANGLE_DEG), f);
    return glm::vec3(rot * glm::vec4(baseUp, 0.0f));
}

glm::mat4 Camera::view() const {
    glm::vec3 e = eyePos();
    return glm::lookAt(e, e + front(), up());
}

glm::mat4 Camera::proj(float aspect) const {
    return glm::perspective(glm::radians(fov), aspect, 0.1f, 900.0f);
}
