#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "RenderContext.h"
#include "SceneManager.h"
#include "SceneRenderer.h"
#include "InputHandler.h"
#include "shader.h"

class Application {
public:
    RenderContext renderCtx;
    SceneManager sceneManager;
    GPUResourceManager gpuResources;
    PathTracerRenderer pathTracerRenderer;
    RasterRenderer rasterRenderer;
    InputHandler inputHandler;

    Shader pathTracerShader, screenShader, rasterShader;

    int currentSceneIndex = 0;
    int frameIndex = 0;
    bool isRendering = false;
    bool renderDone = false;
    int maxBounces = 1;
    int samplesPerPixel = 1;

    std::vector<Triangle> ptTriangles;
    bool ptTrianglesDirty = true;
    std::vector<GPUResourceManager::CPU_Sphere> cpuSpheres;

    float deltaTime = 0.0f, lastFrame = 0.0f;

    bool Initialize() {
        if (!renderCtx.Initialize()) return false;

        // 初始化 ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOpenGL(renderCtx.window, false);
        ImGui_ImplOpenGL3_Init("#version 430");

        // 初始化着色器
        try {
            pathTracerShader = Shader("shaders/path_tracer.comp");
            screenShader = Shader("shaders/quad.vs", "shaders/quad.fs");
            rasterShader = Shader("shaders/raster.vs", "shaders/raster.fs");
        } catch (const std::exception& e) {
            std::cerr << "[Error] Shader loading failed: " << e.what() << "\n";
            return false;
        }

        // 初始化 GPU 资源
        gpuResources.Initialize();
        pathTracerRenderer.Initialize(RenderContext::DEFAULT_WIDTH, RenderContext::DEFAULT_HEIGHT);

        // 设置输入处理
        inputHandler.SetCamera(&sceneManager.camera);
        inputHandler.SetupCallbacks(renderCtx.window);

        // 加载初始场景
        sceneManager.LoadSceneByIndex(0);
        UploadSceneToGPU();

        return true;
    }

    void UploadSceneToGPU() {
        gpuResources.CreateMaterialTextureArray(sceneManager.loader.materials);
        gpuResources.UploadMaterialsSSBO(sceneManager.loader.materials);
        gpuResources.UploadLightsSSBO(sceneManager.loader.lights);
        gpuResources.UploadSpheresSSBO(cpuSpheres);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpuResources.ssboTriangles);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     sceneManager.loader.triangles.size() * sizeof(Triangle),
                     sceneManager.loader.triangles.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        frameIndex = 0;
        renderDone = false;
        ptTrianglesDirty = true;
    }

    void GeneratePathTracingTriangles() {
        if (!ptTrianglesDirty) return;

        ptTriangles.clear();
        for (const auto& mesh : sceneManager.sceneMeshes) {
            for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                Triangle tri;
                uint32_t idx0 = mesh.indices[i];
                uint32_t idx1 = mesh.indices[i + 1];
                uint32_t idx2 = mesh.indices[i + 2];

                tri.v0 = glm::vec4(mesh.vertices[idx0].position, 1.0f);
                tri.v1 = glm::vec4(mesh.vertices[idx1].position, 1.0f);
                tri.v2 = glm::vec4(mesh.vertices[idx2].position, 1.0f);

                tri.n0 = glm::vec4(mesh.vertices[idx0].normal, 0.0f);
                tri.n1 = glm::vec4(mesh.vertices[idx1].normal, 0.0f);
                tri.n2 = glm::vec4(mesh.vertices[idx2].normal, 0.0f);

                tri.uv0 = glm::vec4(mesh.vertices[idx0].uv, 0.0f, 0.0f);
                tri.uv1 = glm::vec4(mesh.vertices[idx1].uv, 0.0f, 0.0f);
                tri.uv2 = glm::vec4(mesh.vertices[idx2].uv, 0.0f, 0.0f);

                tri.material_id = mesh.material_index;
                ptTriangles.push_back(tri);
            }
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpuResources.ssboTriangles);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     ptTriangles.size() * sizeof(Triangle),
                     ptTriangles.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        ptTrianglesDirty = false;
    }

    void UpdateUI() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2((float)RenderContext::SIDEBAR_WIDTH, (float)RenderContext::DEFAULT_HEIGHT));
        ImGui::Begin("Scene Control", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Triangles: %d", (int)sceneManager.loader.triangles.size());
        ImGui::Separator();

        if (ImGui::Checkbox("Enable Path Tracing", &isRendering)) {
            renderDone = false;
            frameIndex = 0;
            pathTracerRenderer.ClearOutput();
            inputHandler.SetRenderingMode(isRendering);
        }

        if (isRendering) {
            ImGui::TextColored(ImVec4(1, 0.5, 0, 1), "Mode: PATH TRACING");
            if (renderDone) {
                ImGui::Text("Status: Render Finished");
                if (ImGui::Button("Re-Render")) renderDone = false;
            } else {
                ImGui::Text("Status: Rendering...");
            }
        } else {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Mode: PREVIEW");
        }

        ImGui::Separator();
        const char* items[] = { "Slime", "Cube", "Cornell Box", "Breakfast_room" };
        if (ImGui::Combo("Select Scene", &currentSceneIndex, items, 4)) {
            sceneManager.LoadSceneByIndex(currentSceneIndex);
            UploadSceneToGPU();
            renderDone = false;
            isRendering = false;
            inputHandler.SetRenderingMode(false);
        }

        ImGui::SliderFloat("FOV", &sceneManager.camera.Zoom, 1.0f, 3000.0f);
        ImGui::Text("Camera Speed: %.3f", sceneManager.camera.MovementSpeed);
        ImGui::SliderInt("Max Bounces", &maxBounces, 0, 8);
        ImGui::SliderInt("Samples Per Pixel", &samplesPerPixel, 1, 10000);

        if (ImGui::Button("Reset View")) {
            sceneManager.LoadSceneByIndex(currentSceneIndex);
            UploadSceneToGPU();
            renderDone = false;
        }

        if (ImGui::Button("Load SmallPT Scene")) {
            sceneManager.SetupSmallPTScene(cpuSpheres);
            gpuResources.UploadSpheresSSBO(cpuSpheres);
            gpuResources.ClearTriangleSSBO();
            frameIndex = 0;
            renderDone = false;
        }

        ImGui::End();
    }

    void Render() {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (isRendering) {
            glViewport(RenderContext::SIDEBAR_WIDTH, 0,
                       RenderContext::DEFAULT_WIDTH - RenderContext::SIDEBAR_WIDTH,
                       RenderContext::DEFAULT_HEIGHT);

            GeneratePathTracingTriangles();

            if (!renderDone) {
                pathTracerShader.use();
                pathTracerShader.setInt("u_texArray", 3);
                pathTracerShader.setInt("u_light_count", (int)sceneManager.loader.lights.size());
                pathTracerShader.setInt("u_triangle_count", (int)ptTriangles.size());
                pathTracerShader.setInt("u_material_count", (int)sceneManager.loader.materials.size());
                pathTracerShader.setInt("u_frame_index", frameIndex);
                pathTracerShader.setInt("u_samples_per_pixel", samplesPerPixel);
                pathTracerShader.setInt("u_max_bounces", maxBounces);

                float aspect = float(RenderContext::DEFAULT_WIDTH - RenderContext::SIDEBAR_WIDTH) /
                               float(RenderContext::DEFAULT_HEIGHT);
                pathTracerShader.setMat4("u_inv_view", sceneManager.camera.GetInverseViewMatrix());
                pathTracerShader.setMat4("u_inv_proj", sceneManager.camera.GetInverseProjectionMatrix(aspect));

                glBindImageTexture(0, pathTracerRenderer.GetOutputTexture(), 0, GL_FALSE, 0,
                                   GL_READ_WRITE, GL_RGBA32F);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gpuResources.ssboTriangles);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gpuResources.ssboMaterials);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, gpuResources.ssboSpheres);

                if (gpuResources.texArray != 0) {
                    glActiveTexture(GL_TEXTURE3);
                    glBindTexture(GL_TEXTURE_2D_ARRAY, gpuResources.texArray);
                }

                GLuint groupX = (RenderContext::DEFAULT_WIDTH + 15) / 16;
                GLuint groupY = (RenderContext::DEFAULT_HEIGHT + 15) / 16;
                glDispatchCompute(groupX, groupY, 1);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

                frameIndex++;
                if (frameIndex >= samplesPerPixel) {
                    renderDone = true;
                }
            }

            pathTracerRenderer.DisplayToScreen(screenShader);
        } else {
            glViewport(0, 0, RenderContext::DEFAULT_WIDTH, RenderContext::DEFAULT_HEIGHT);
            rasterRenderer.Render(sceneManager.camera, rasterShader, sceneManager.sceneMeshes,
                                  sceneManager.loader.lights,
                                  RenderContext::DEFAULT_WIDTH, RenderContext::DEFAULT_HEIGHT);
        }
    }

    void Run() {
        while (renderCtx.IsRunning()) {
            float currentFrame = (float)glfwGetTime();
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            // 处理输入
            if (glfwGetKey(renderCtx.window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(renderCtx.window, true);
            }

            if (!ImGui::GetIO().WantCaptureKeyboard && !isRendering) {
                float speed = sceneManager.camera.MovementSpeed * deltaTime;
                if (glfwGetKey(renderCtx.window, GLFW_KEY_W) == GLFW_PRESS)
                    sceneManager.camera.Position += sceneManager.camera.Forward * speed;
                if (glfwGetKey(renderCtx.window, GLFW_KEY_S) == GLFW_PRESS)
                    sceneManager.camera.Position -= sceneManager.camera.Forward * speed;
                if (glfwGetKey(renderCtx.window, GLFW_KEY_A) == GLFW_PRESS)
                    sceneManager.camera.Position -=
                        glm::normalize(glm::cross(sceneManager.camera.Forward, sceneManager.camera.Up)) * speed;
                if (glfwGetKey(renderCtx.window, GLFW_KEY_D) == GLFW_PRESS)
                    sceneManager.camera.Position +=
                        glm::normalize(glm::cross(sceneManager.camera.Forward, sceneManager.camera.Up)) * speed;
            }

            UpdateUI();
            Render();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            renderCtx.SwapBuffers();
            renderCtx.PollEvents();
        }
    }

    void Shutdown() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        renderCtx.Shutdown();
    }
};

int main() {
    Application app;
    if (!app.Initialize()) {
        std::cerr << "Failed to initialize application\n";
        return 1;
    }

    app.Run();
    app.Shutdown();

    return 0;
}

