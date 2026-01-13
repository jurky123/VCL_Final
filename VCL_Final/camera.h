#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    glm::vec3 Position = glm::vec3(0.0f, 0.0f, 5.0f);
    glm::vec3 Forward = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 Up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 Right = glm::normalize(glm::cross(Forward, Up));

    float Yaw = -90.0f;
    float Pitch = 0.0f;

    float MovementSpeed = 5.0f;
    float MouseSensitivity = 0.03f;
    float Zoom = 45.0f; // 光栅化和路径追踪共用

    Camera() {}

    // ---------------- 路径追踪使用 ----------------
    glm::mat4 GetInverseViewMatrix() const {
        return glm::inverse(glm::lookAt(Position, Position + Forward, Up));
    }

    glm::mat4 GetInverseProjectionMatrix(float aspectRatio) const {
        return glm::inverse(glm::perspective(glm::radians(Zoom), aspectRatio, 0.1f, 1000.0f));
    }

    // ---------------- 光栅化预览使用 ----------------
    glm::mat4 GetViewMatrix() const {
        return glm::lookAt(Position, Position + Forward, Up);
    }

    glm::mat4 GetProjectionMatrix(float aspectRatio) const {
        return glm::perspective(glm::radians(Zoom), aspectRatio, 0.1f, 1000.0f);
    }

    // ---------------- 输入控制 ----------------
    void ProcessKeyboard(const glm::vec3& direction, float deltaTime) {
        float velocity = MovementSpeed * deltaTime;
        Position += direction * velocity;
    }

    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true) {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;
        Yaw += xoffset;
        Pitch += yoffset;

        if (constrainPitch) {
            if (Pitch > 89.0f)  Pitch = 89.0f;
            if (Pitch < -89.0f) Pitch = -89.0f;
        }

        updateCameraVectors();
    }

    void ProcessMouseScroll(float yoffset) {
        Zoom -= yoffset;
        if (Zoom < 1.0f)  Zoom = 1.0f;
        if (Zoom > 90.0f) Zoom = 90.0f;
    }

private:
    void updateCameraVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Forward = glm::normalize(front);

        // 重新计算右向量和上向量
        Right = glm::normalize(glm::cross(Forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        Up = glm::normalize(glm::cross(Right, Forward));
    }
};

#endif