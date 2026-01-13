#pragma once
#include <glm/glm.hpp>
#include <cstdint>

// Triangle 结构体，只定义一次
struct Triangle {
    glm::vec4 v0, v1, v2;      // 顶点位置
    glm::vec3 n0, n1, n2;      // 顶点法线
    glm::vec2 uv0, uv1, uv2;   // 顶点 UV
    uint32_t material_id;
    uint32_t pad0, pad1, pad2; // padding
};
static_assert(sizeof(Triangle) == 124, "Triangle layout mismatch!");
