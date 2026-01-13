#pragma once
#include <vector>
#include "triangle.h"
#include "material.h"

struct Scene {
    std::vector<Triangle> triangles;
    std::vector<Material> materials;

    int addMaterial(const Material& mat) {
        materials.push_back(mat);
        return int(materials.size() - 1);
    }
};
