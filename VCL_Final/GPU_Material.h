#pragma once
#include <glm/glm.hpp>
#include "material.h"

struct GPU_Material {
    glm::vec3 baseColor;
    float emission;
    glm::vec3 specular;
    float metallic;
    float roughness;
    int diffuseTexID;
    int pad0;
    int pad1;
};

// ´Ó CPU Material ×ªÎª GPU_Material
inline GPU_Material ConvertToGPU(const Material& mat) {
    GPU_Material gpuMat;
    gpuMat.baseColor = mat.diffuse;
    gpuMat.emission = mat.emission;
    gpuMat.specular = mat.specular;
    gpuMat.metallic = mat.metallic;
    gpuMat.roughness = mat.shiness;
    gpuMat.diffuseTexID = mat.diffuseTexID;
    gpuMat.pad0 = gpuMat.pad1 = 0;
    return gpuMat;
}