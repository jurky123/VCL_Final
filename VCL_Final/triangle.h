#pragma once
#include <glm/glm.hpp>
#include <cstdint>

// Triangle 结构体，只定义一次
struct Triangle {
    glm::vec4 v0;
    glm::vec4 v1;
    glm::vec4 v2;
    uint32_t material_id;
    uint32_t pad0, pad1, pad2; // padding 到 64B
};
static_assert(sizeof(Triangle) == 64, "Triangle layout mismatch!");
