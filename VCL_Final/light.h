#pragma once
#include "material.h"

struct Light {
    enum Type { Point, Directional } type;
    glm::vec3 position;   // 对点光
    glm::vec3 direction;  // 对方向光
    glm::vec3 intensity;
};
