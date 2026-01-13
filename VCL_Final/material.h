#pragma once
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
// ----------- Material 类 -------------
struct Material {
    glm::vec3 baseColor = glm::vec3(1.0f); // 漫反射颜色
    float emission = 0.0f;                 // 发光强度

    float metallic = 0.0f;                 // 金属度
    float roughness = 0.5f;                // 粗糙度

    // 构造函数方便直接初始化
    Material() = default;
    Material(const glm::vec3& color, float emit = 0.0f, float met = 0.0f, float rough = 0.5f)
        : baseColor(color), emission(emit), metallic(met), roughness(rough) {
    }
};
