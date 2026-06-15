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

float Camera::aimYaw() const { return yaw + recoilYaw; }

float Camera::aimPitch() const {
    float p = pitch + recoilPitch;
    if (p >  89.0f) p =  89.0f;
    if (p < -89.0f) p = -89.0f;
    return p;
}

void Camera::applyRecoil(float dPitch, float dYaw) {
    recoilPitch += dPitch;
    if (recoilPitch > RECOIL_PITCH_CAP) recoilPitch = RECOIL_PITCH_CAP;
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

glm::mat4 Camera::view() const {
    return glm::lookAt(eye, eye + front(), glm::vec3(0, 1, 0));
}

glm::mat4 Camera::proj(float aspect) const {
    return glm::perspective(glm::radians(fov), aspect, 0.1f, 500.0f);
}
