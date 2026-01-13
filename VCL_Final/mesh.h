#pragma once
#include"material.h"
#include<vector>
struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct Mesh {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    Material material;   // π‚’§ªØ”√
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;
    size_t indexCount = 0;

    void setupGL();
};

