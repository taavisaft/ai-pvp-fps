#include "camera.h"
#include <glm/gtc/matrix_transform.hpp>

static constexpr float SENSITIVITY = 0.1f;

void Camera::addLook(float xrel, float yrel) {
    yaw   += xrel * SENSITIVITY;
    pitch -= yrel * SENSITIVITY;
    if (pitch >  89.0f) pitch =  89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}

glm::vec3 Camera::front() const {
    return glm::normalize(glm::vec3(
        cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
        sin(glm::radians(pitch)),
        sin(glm::radians(yaw)) * cos(glm::radians(pitch))
    ));
}

glm::mat4 Camera::view() const {
    return glm::lookAt(eye, eye + front(), glm::vec3(0, 1, 0));
}

glm::mat4 Camera::proj(float aspect) const {
    return glm::perspective(glm::radians(75.0f), aspect, 0.1f, 500.0f);
}
