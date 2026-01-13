#include "material.h"
bool Material::LoadMTL(const std::string& path, std::unordered_map<std::string, Material>& outMaterials) {
    std::ifstream fin(path);
    if (!fin.is_open()) {
        std::cerr << "Failed to open MTL: " << path << std::endl;
        return false;
    }

    std::string line;
    Material currentMat;
    std::string currentName;

    while (std::getline(fin, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "newmtl") {
            if (!currentName.empty()) outMaterials[currentName] = currentMat;
            iss >> currentName;
            currentMat = Material(); // 重置
        }
        else if (cmd == "Ka") {
            iss >> currentMat.ambient.r >> currentMat.ambient.g >> currentMat.ambient.b;
        }
        else if (cmd == "Kd") {
            iss >> currentMat.diffuse.r >> currentMat.diffuse.g >> currentMat.diffuse.b;
        }
        else if (cmd == "Ks") {
            iss >> currentMat.specular.r >> currentMat.specular.g >> currentMat.specular.b;
        }
        else if (cmd == "Ns") {
            iss >> currentMat.shiness;
        }
        else if (cmd == "map_Kd") {
            iss >> currentMat.diffuseTexPath;
        }
        else if (cmd == "d") { /* 可以解析透明度 */ }
    }

    if (!currentName.empty()) outMaterials[currentName] = currentMat;
    return true;
}