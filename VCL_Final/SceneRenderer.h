#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "shader.h"
#include "camera.h"
#include "ModelLoader.h"
#include "Sphere.h"

// 管理纹理、材质、灯光、球体的 SSBO 上传
class GPUResourceManager {
public:
    GLuint ssboTriangles = 0;
    GLuint ssboMaterials = 0;
    GLuint ssboLights = 0;
    GLuint ssboSpheres = 0;
    GLuint texArray = 0;

    using CPU_Sphere = ::CPU_Sphere;

    GPUResourceManager();
    ~GPUResourceManager();

    void Initialize();
    // Allow modification of materials (assign diffuseTexID), take non-const reference
    void CreateMaterialTextureArray(std::vector<Material>& materials);
    void UploadMaterialsSSBO(const std::vector<Material>& materials);
    void UploadLightsSSBO(const std::vector<Light>& lights);
    void UploadSpheresSSBO(const std::vector<CPU_Sphere>& spheres);
    void ClearTriangleSSBO();

private:
    void CreateBuffers();
};

// 路径追踪渲染管理
class PathTracerRenderer {
public:
    PathTracerRenderer();
    ~PathTracerRenderer();

    void Initialize(unsigned int screenWidth, unsigned int screenHeight);
    void Render(const Camera& camera, const Shader& shader, const ModelLoader& loader,
                const std::vector<Triangle>& ptTriangles, int frameIndex, 
                bool renderDone, int& outFrameIndex);
    void DisplayToScreen(const Shader& screenShader);
    unsigned int GetOutputTexture() const { return texOutput; }
    void ClearOutput();

    unsigned int GetWidth() const { return screenWidth; }
    unsigned int GetHeight() const { return screenHeight; }

private:
    unsigned int texOutput = 0;
    unsigned int quadVAO = 0, quadVBO = 0;
    unsigned int screenWidth = 0, screenHeight = 0;

    void CreateQuadMesh();
    void CreateOutputTexture();
};

// 光栅化渲染管理
class RasterRenderer {
public:
    RasterRenderer();
    ~RasterRenderer();

    void Render(const Camera& camera, const Shader& shader, const std::vector<Mesh>& meshes,
                const std::vector<Light>& lights, unsigned int screenWidth, unsigned int screenHeight);
};