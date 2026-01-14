#pragma once
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include<iostream>
// ----------- Material 类 -------------
struct Material {
    glm::vec3 diffuse = glm::vec3(1.0f); // 漫反射颜色
    glm::vec3 specular = glm::vec3(1.0f);
    float emission = 0.0f;                 // 发光强度
    glm::vec3 ambient = glm::vec3(0.0f);
    float metallic = 0.0f;                 // 金属度
    float shiness = 0.4f;                // 粗糙度
    std::string diffuseTexPath;
    unsigned int diffuseTexID = 0;
    GLuint diffuseTex = 0; // 新增
    int id = -1; // 新增，用于唯一标识
    // 构造函数方便直接初始化
    Material() = default;
    Material(const glm::vec3& color, float emit = 0.0f, float met = 0.0f, float shiness = 0.5f,const glm::vec3& specular = glm::vec3(1.0f))
        : diffuse(color), emission(emit), metallic(met), shiness(shiness) ,specular(specular){
    }
    bool LoadMTL(const std::string& path, std::unordered_map<std::string, Material>& outMaterials);
};