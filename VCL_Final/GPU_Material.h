#pragma once
#include <glm/glm.hpp>
#include "material.h"

struct alignas(16) GPU_Material {
    glm::vec4 diffuse;
    glm::vec4 specular;

    float emission;
    float metallic;
    float shiness;
    int diffuseTexID;
};
static_assert(sizeof(GPU_Material) == 48);

// ´Ó CPU Material ×ªÎª GPU_Material