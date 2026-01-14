#pragma once
#include <glm/glm.hpp>
#include <cstdint>

// Triangle 结构体，只定义一次
struct Triangle {
    glm::vec4 v0;  // 顶点位置，w=1.0
    glm::vec4 v1;
    glm::vec4 v2;

    glm::vec4 n0;  // 顶点法线，w=0
    glm::vec4 n1;
    glm::vec4 n2;

    glm::vec4 uv0; // xy = uv，zw = 0
    glm::vec4 uv1;
    glm::vec4 uv2;

    uint32_t material_id;
    uint32_t pad0, pad1, pad2; // 对齐到 16 bytes
};

