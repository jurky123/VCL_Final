#pragma once
#include <glm/glm.hpp>

// CPU-side sphere representation used by UI and SSBO uploads
struct CPU_Sphere {
    glm::vec4 pos; // xyz = center, w = radius
    glm::vec4 emission; // rgb + padding
    glm::vec4 color; // rgb + padding
    int type; // 0=DIFF,1=SPEC,2=REFR
    float ior;
    int pad0;
    int pad1;
};
