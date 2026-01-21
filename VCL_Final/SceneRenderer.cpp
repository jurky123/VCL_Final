#include "SceneRenderer.h"
#include "GPU_Material.h"
#include <iostream>
#include <algorithm>

// ============== GPUResourceManager ==============

GPUResourceManager::GPUResourceManager() {}

GPUResourceManager::~GPUResourceManager() {
    if (ssboTriangles) glDeleteBuffers(1, &ssboTriangles);
    if (ssboMaterials) glDeleteBuffers(1, &ssboMaterials);
    if (ssboLights) glDeleteBuffers(1, &ssboLights);
    if (ssboSpheres) glDeleteBuffers(1, &ssboSpheres);
    if (texArray) glDeleteTextures(1, &texArray);
}

void GPUResourceManager::Initialize() {
    CreateBuffers();
}

void GPUResourceManager::CreateBuffers() {
    if (ssboTriangles == 0) {
        glGenBuffers(1, &ssboTriangles);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboTriangles);
    }
    if (ssboMaterials == 0) {
        glGenBuffers(1, &ssboMaterials);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboMaterials);
    }
    if (ssboLights == 0) {
        glGenBuffers(1, &ssboLights);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, ssboLights);
    }
    if (ssboSpheres == 0) {
        glGenBuffers(1, &ssboSpheres);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, ssboSpheres);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void GPUResourceManager::CreateMaterialTextureArray(std::vector<Material>& materials) {
    int layerIndex = 1;
    for (auto& m : materials) {
        if (!m.diffuseTexPath.empty()) {
            m.diffuseTexID = layerIndex++;
        } else {
            m.diffuseTexID = 0;
        }
    }

    int texCount = layerIndex - 1;
    if (texCount == 0) return;

    int maxWidth = 0, maxHeight = 0;
    std::vector<unsigned char*> loadedData(texCount, nullptr);

    for (auto& m : materials) {
        if (m.diffuseTexID == 0) continue;

        int w, h, n;
        unsigned char* data = stbi_load(m.diffuseTexPath.c_str(), &w, &h, &n, 4);
        if (!data) {
            std::cout << "[WARN] Failed to load texture: " << m.diffuseTexPath << "\n";
            m.diffuseTexID = 0;
            continue;
        }
        int layer = m.diffuseTexID - 1;
        loadedData[layer] = data;
        if (w > maxWidth) maxWidth = w;
        if (h > maxHeight) maxHeight = h;
    }

    glGenTextures(1, &texArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texArray);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, maxWidth, maxHeight, texCount);

    for (auto& m : materials) {
        if (m.diffuseTexID == 0) continue;

        int layer = m.diffuseTexID - 1;
        unsigned char* data = loadedData[layer];
        if (!data) continue;

        int w, h, n;
        stbi_info(m.diffuseTexPath.c_str(), &w, &h, &n);

        std::vector<unsigned char> layerData(maxWidth * maxHeight * 4, 255);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                for (int c = 0; c < 4; ++c) {
                    layerData[(y * maxWidth + x) * 4 + c] = data[(y * w + x) * 4 + c];
                }
            }
        }

        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, maxWidth, maxHeight, 1,
                        GL_RGBA, GL_UNSIGNED_BYTE, layerData.data());

        stbi_image_free(data);
    }

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    std::cout << "[TextureArray] Created with " << texCount << " layers, maxSize: "
              << maxWidth << "x" << maxHeight << "\n";
}

void GPUResourceManager::UploadMaterialsSSBO(const std::vector<Material>& materials) {
    std::vector<GPU_Material> gpuMats;
    gpuMats.reserve(materials.size());

    for (const auto& m : materials) {
        GPU_Material gm{};
        gm.diffuse = glm::vec4(m.diffuse, 0.0f);
        gm.specular = glm::vec4(m.specular, 0.0f);
        gm.emission = m.emission;
        gm.metallic = m.metallic;
        gm.shiness = m.shiness;
        gm.diffuseTexID = static_cast<int>(m.diffuseTexID);
        gpuMats.push_back(gm);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboMaterials);
    glBufferData(GL_SHADER_STORAGE_BUFFER, gpuMats.size() * sizeof(GPU_Material),
                 gpuMats.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboMaterials);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    std::cout << "[SSBO] Uploaded " << gpuMats.size() << " materials\n";
}

void GPUResourceManager::UploadLightsSSBO(const std::vector<Light>& lights) {
    struct GPU_Light_CPU {
        int type;
        int pad0, pad1, pad2;
        glm::vec4 position;
        glm::vec4 direction;
        glm::vec4 intensity;
    };

    std::vector<GPU_Light_CPU> gpuLights;
    gpuLights.reserve(lights.size());

    for (const auto& l : lights) {
        GPU_Light_CPU gl;
        gl.type = (l.type == Light::Point) ? 0 : 1;
        gl.pad0 = gl.pad1 = gl.pad2 = 0;
        gl.position = glm::vec4(l.position, 1.0f);
        gl.direction = glm::vec4(glm::normalize(l.direction), 0.0f);
        gl.intensity = glm::vec4(l.intensity, 0.0f);
        gpuLights.push_back(gl);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboLights);
    if (!gpuLights.empty()) {
        glBufferData(GL_SHADER_STORAGE_BUFFER, gpuLights.size() * sizeof(GPU_Light_CPU),
                     gpuLights.data(), GL_STATIC_DRAW);
    } else {
        glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_STATIC_DRAW);
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, ssboLights);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    std::cout << "[SSBO] Uploaded " << gpuLights.size() << " lights\n";
}

void GPUResourceManager::UploadSpheresSSBO(const std::vector<CPU_Sphere>& spheres) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboSpheres);
    if (!spheres.empty()) {
        glBufferData(GL_SHADER_STORAGE_BUFFER, spheres.size() * sizeof(CPU_Sphere),
                     spheres.data(), GL_STATIC_DRAW);
    } else {
        glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_STATIC_DRAW);
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, ssboSpheres);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    std::cout << "[SSBO] Uploaded " << spheres.size() << " spheres\n";
}

void GPUResourceManager::ClearTriangleSSBO() {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboTriangles);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_STATIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// ============== PathTracerRenderer ==============

PathTracerRenderer::PathTracerRenderer() {}

PathTracerRenderer::~PathTracerRenderer() {
    if (texOutput) glDeleteTextures(1, &texOutput);
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO) glDeleteBuffers(1, &quadVBO);
}

void PathTracerRenderer::Initialize(unsigned int width, unsigned int height) {
    screenWidth = width;
    screenHeight = height;
    CreateOutputTexture();
    CreateQuadMesh();
}

void PathTracerRenderer::CreateOutputTexture() {
    glGenTextures(1, &texOutput);
    glBindTexture(GL_TEXTURE_2D, texOutput);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, screenWidth, screenHeight, 0,
                 GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void PathTracerRenderer::CreateQuadMesh() {
    float quadVertices[] = {
        -1,  1, 0, 0, 1,
        -1, -1, 0, 0, 0,
         1, -1, 0, 1, 0,
        -1,  1, 0, 0, 1,
         1, -1, 0, 1, 0,
         1,  1, 0, 1, 1
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

void PathTracerRenderer::ClearOutput() {
    glBindTexture(GL_TEXTURE_2D, texOutput);
    std::vector<float> zeros(screenWidth * screenHeight * 4, 0.0f);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, screenWidth, screenHeight, GL_RGBA, GL_FLOAT, zeros.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void PathTracerRenderer::DisplayToScreen(const Shader& screenShader) {
    // remove const to call non-const use()
    screenShader.use();
    glBindVertexArray(quadVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texOutput);
    screenShader.setInt("u_texOutput", 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void PathTracerRenderer::Render(const Camera& camera, const Shader& shader,
                                const ModelLoader& loader, const std::vector<Triangle>& ptTriangles,
                                int frameIndex, bool renderDone, int& outFrameIndex) {
    if (renderDone) return;

    shader.use();
    shader.setInt("u_texArray", 3);
    shader.setInt("u_light_count", (int)loader.lights.size());
    shader.setInt("u_triangle_count", (int)ptTriangles.size());
    shader.setInt("u_material_count", (int)loader.materials.size());
    shader.setInt("u_frame_index", frameIndex);
    shader.setInt("u_max_bounces", 1);  // TODO: 参数化
    shader.setInt("u_samples_per_pixel", 1);

    float aspect = 900.0f / 800.0f;  // TODO: 参数化
    shader.setMat4("u_inv_view", camera.GetInverseViewMatrix());
    shader.setMat4("u_inv_proj", camera.GetInverseProjectionMatrix(aspect));

    glBindImageTexture(0, texOutput, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

    GLuint groupX = (screenWidth + 15) / 16;
    GLuint groupY = (screenHeight + 15) / 16;
    glDispatchCompute(groupX, groupY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    outFrameIndex++;
}

// ============== RasterRenderer ==============

RasterRenderer::RasterRenderer() {}

RasterRenderer::~RasterRenderer() {}

void RasterRenderer::Render(const Camera& camera, const Shader& shader,
                           const std::vector<Mesh>& meshes, const std::vector<Light>& lights,
                           unsigned int screenWidth, unsigned int screenHeight) {
    glEnable(GL_DEPTH_TEST);
    shader.use();
    shader.setMat4("view", camera.GetViewMatrix());
    shader.setMat4("proj", glm::perspective(glm::radians(camera.Zoom),
                   float(screenWidth) / screenHeight, 0.1f, 10000.0f));
    shader.setVec3("u_cam_pos", camera.Position);
    shader.setVec3("u_ambient", glm::vec3(0.1f));

    int light_count = std::min((int)lights.size(), 16);
    shader.setInt("u_light_count", light_count);
    for (int i = 0; i < light_count; i++) {
        shader.setVec3("u_light_positions[" + std::to_string(i) + "]", lights[i].position);
        shader.setVec3("u_light_intensities[" + std::to_string(i) + "]", lights[i].intensity);
    }

    for (const auto& mesh : meshes) {
        shader.setVec3("diffuse", mesh.material.diffuse);
        shader.setVec3("specular", mesh.material.specular);
        shader.setFloat("shiness", mesh.material.shiness);
        shader.setFloat("emission", mesh.material.emission);

        if (mesh.material.diffuseTex) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mesh.material.diffuseTex);
            shader.setInt("u_diffuseTex", 0);
            shader.setInt("hasDiffuseTex", 1);
        } else {
            shader.setInt("hasDiffuseTex", 0);
        }

        glBindVertexArray(mesh.VAO);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
    glDisable(GL_DEPTH_TEST);
}