#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>
#include <filesystem>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <yaml-cpp/yaml.h>
#include "mesh.h"
#include "Triangle.h"
#include "Material.h"
#include "light.h"

#include "stb_image.h"
class ModelLoader {
public:
    std::vector<Triangle> triangles;
    std::vector<Material> materials;
    std::vector<Light> lights;
    glm::vec3 initialEye = glm::vec3(0, 0, 5);
    glm::vec3 initialTarget = glm::vec3(0, 0, 0);
    float initialFovy = 45.0f;
    std::unordered_map<std::string, GLuint> loadedTextures;
    std::unordered_map<std::string, uint32_t> materialNameToID;
    uint32_t nextMaterialID = 0;

    static inline glm::mat4 aiMatrix4x4ToGlm(const aiMatrix4x4& from) {
        glm::mat4 to;
        to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
        to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
        to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
        to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
        return to;
    }

    bool LoadModel(const std::string& path,
        const glm::mat4& transform = glm::mat4(1.0f),
        uint32_t defaultMaterialID = UINT32_MAX, // 默认值表示未指定
        bool forceMaterial = true); // forceMaterial 参数被添加到此处
        
    bool LoadSceneFromYAML(const std::string& yamlPath);

    void SetupRasterMesh(const std::vector<Triangle>& tris, const Material& mat, Mesh& mesh);

    void CreateRasterMeshes(std::vector<Mesh>& meshes);

    Mesh ConvertToMesh(
        const std::vector<Triangle>& tris,
        int material_index
    );
    bool LoadMTL(const std::string& path, std::unordered_map<std::string, Material>& outMaterials);


    GLuint LoadTexture(const std::string& path);
private:
    void ProcessNode(aiNode* node,
        const aiScene* scene,
        const glm::mat4& transform,
        uint32_t defaultMaterialID,
        const std::unordered_map<std::string, Material>& mtlMaterials,
        bool forceMaterial = true);

    void ExtractTriangles(aiMesh* mesh,
        const glm::mat4& transform,
        uint32_t materialID);
};
