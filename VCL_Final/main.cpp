#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <string>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "material.h"
#include "shader.h"
#include "camera.h"
#include "ModelLoader.h"

// --- 配置与全局变量 ---
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 800;
const int SIDEBAR_WIDTH = 300;
GLuint ssboMaterials = 0;

Camera camera;
float lastX = (SCR_WIDTH - SIDEBAR_WIDTH) / 2.0f + SIDEBAR_WIDTH;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
int frameIndex = 0;
bool isRendering = false; // 初始为预览模式

// 场景与 SSBO
unsigned int ssboTriangles;
ModelLoader loader;
std::vector<Mesh> sceneMeshes;
std::vector<std::string> scenePaths = {
    "models/slime.obj",
    "models/cube.obj",
    "models/cornell_box/cornell_box.yaml",
    "models/breakfast_room/breakfast_room.yaml"
};
int currentSceneIndex = 0;

// --- 核心函数：加载场景到 GPU ---
void LoadScene(const std::string& path) {
    bool success = false;
    std::string ext = "";
    size_t dotPos = path.find_last_of(".");
    if (dotPos != std::string::npos) ext = path.substr(dotPos + 1);

    if (ext == "yaml") {
        success = loader.LoadSceneFromYAML(path);
        if (success) {
            camera.Position = loader.initialEye;
            camera.Forward = glm::normalize(loader.initialTarget - loader.initialEye);
            camera.Zoom = loader.initialFovy;
            camera.Yaw = glm::degrees(atan2(camera.Forward.z, camera.Forward.x));
            camera.Pitch = glm::degrees(asin(camera.Forward.y));
        }
    }
    else {
        loader.triangles.clear();
        success = loader.LoadModel(path);
        if (success) {
            camera.Position = glm::vec3(0, 0, 5);
            camera.Forward = glm::vec3(0, 0, -1);
            camera.Yaw = -90.0f;
            camera.Pitch = 0.0f;
            camera.Zoom = 45.0f;
        }
    }

    if (success) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboTriangles);
        glBufferData(GL_SHADER_STORAGE_BUFFER, loader.triangles.size() * sizeof(Triangle), loader.triangles.data(), GL_STATIC_DRAW);
        frameIndex = 0;
        std::cout << "Successfully loaded: " << path << " (" << loader.triangles.size() << " triangles)" << std::endl;
    }
}

// --- 回调函数 ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(SIDEBAR_WIDTH, 0, width - SIDEBAR_WIDTH, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    if (ImGui::GetIO().WantCaptureMouse || isRendering) return; // 渲染模式下不响应旋转
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos; lastY = ypos;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        camera.ProcessMouseMovement(xoffset, yoffset);
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (ImGui::GetIO().WantCaptureMouse || isRendering) return; // 渲染模式下不响应缩放

    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        camera.Zoom -= (float)yoffset * 2.0f;
        if (camera.Zoom < 1.0f) camera.Zoom = 1.0f;
        if (camera.Zoom > 720.0f) camera.Zoom = 720.0f;
    }
    else {
        if (yoffset > 0) camera.MovementSpeed *= 1.2f;
        else camera.MovementSpeed /= 1.2f;
        if (camera.MovementSpeed < 0.001f) camera.MovementSpeed = 0.001f;
    }
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
    if (ImGui::GetIO().WantCaptureKeyboard || isRendering) return; // 渲染模式下不响应移动

    float currentSpeed = camera.MovementSpeed * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.Position += camera.Forward * currentSpeed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.Position -= camera.Forward * currentSpeed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.Position -= glm::normalize(glm::cross(camera.Forward, camera.Up)) * currentSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.Position += glm::normalize(glm::cross(camera.Forward, camera.Up)) * currentSpeed;
}

int main() {

    // 1. GLFW & GLAD 初始化
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(
        SCR_WIDTH, SCR_HEIGHT,
        "Path Tracer Lab - Offline Mode",
        NULL, NULL
    );
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    // 2. ImGui 初始化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");

    // 3. Shader & SSBO
    Shader pathTracerShader("shaders/path_tracer.comp");
    Shader screenShader("shaders/quad.vs", "shaders/quad.fs");
    Shader rasterShader("shaders/raster.vs", "shaders/raster.fs");
    // --- Triangle SSBO ---
    glGenBuffers(1, &ssboTriangles);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboTriangles);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboTriangles);

    // --- Material SSBO（新增，仅此一处）---
    glGenBuffers(1, &ssboMaterials);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboMaterials);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboMaterials);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // 加载场景（会填充 loader.triangles 和 materials）


    // 加载场景
    LoadScene(scenePaths[currentSceneIndex]);

    // 创建光栅化 Mesh
    loader.CreateRasterMeshes(sceneMeshes);
    // debug
    std::cout << "[DEBUG] Created " << sceneMeshes.size() << " raster meshes" << std::endl;
    for (auto& mesh : sceneMeshes) {
        std::cout << "Mesh indices: " << mesh.indexCount
            << " baseColor: (" << mesh.material.baseColor.r << ","
            << mesh.material.baseColor.g << ","
            << mesh.material.baseColor.b << ")" << std::endl;
    }

    // === 上传 Triangle ===
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboTriangles);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        loader.triangles.size() * sizeof(Triangle),
        loader.triangles.data(),
        GL_STATIC_DRAW
    );

    // === 上传 Material（新增）===
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboMaterials);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        loader.materials.size() * sizeof(Material),
        loader.materials.data(),
        GL_STATIC_DRAW
    );
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // 4. 全屏 Quad
    float quadVertices[] = {
        -1,  1, 0, 0, 1,
        -1, -1, 0, 0, 0,
         1, -1, 0, 1, 0,
        -1,  1, 0, 0, 1,
         1, -1, 0, 1, 0,
         1,  1, 0, 1, 1
    };

    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    // 5. 输出纹理
    unsigned int texOutput;
    glGenTextures(1, &texOutput);
    glBindTexture(GL_TEXTURE_2D, texOutput);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,
        SCR_WIDTH, SCR_HEIGHT,
        0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindImageTexture(0, texOutput, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);


    static bool renderDone = false;

    // 6. 渲染循环
    while (!glfwWindowShouldClose(window)) {

        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // --- ImGui ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        {
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(ImVec2((float)SIDEBAR_WIDTH, (float)SCR_HEIGHT));
            ImGui::Begin("Scene Control", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::Text("Triangles: %d", (int)loader.triangles.size());
            ImGui::Separator();

            if (ImGui::Checkbox("Enable Path Tracing", &isRendering))
                renderDone = false;

            if (isRendering) {
                ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Mode: PATH TRACING");
                if (renderDone) {
                    ImGui::Text("Status: Render Finished");
                    if (ImGui::Button("Re-Render")) renderDone = false;
                }
                else {
                    ImGui::Text("Status: Rendering...");
                }
            }
            else {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Mode: PREVIEW");
            }

            ImGui::Separator();
            const char* items[] = { "Slime", "Cube", "Cornell Box","Breakfast_room"};
            if (ImGui::Combo("Select Scene", &currentSceneIndex, items, 4)) {
                LoadScene(scenePaths[currentSceneIndex]);
                renderDone = false;
            }

            ImGui::SliderFloat("FOV", &camera.Zoom, 1.0f, 720.0f);
            ImGui::Text("Camera Speed: %.3f", camera.MovementSpeed);

            if (ImGui::Button("Reset View")) {
                LoadScene(scenePaths[currentSceneIndex]);
                renderDone = false;
            }

            ImGui::End();
        }

        // 7. Compute Shader
        pathTracerShader.use();
        pathTracerShader.setInt("u_triangle_count", (int)loader.triangles.size());
        pathTracerShader.setInt("u_material_count", (int)loader.materials.size());

        float aspect = float(SCR_WIDTH - SIDEBAR_WIDTH) / float(SCR_HEIGHT);
        pathTracerShader.setMat4("u_inv_view", camera.GetInverseViewMatrix());
        pathTracerShader.setMat4("u_inv_proj", camera.GetInverseProjectionMatrix(aspect));

        if (isRendering) {
            if (!renderDone) {
                // 路径追踪模式
                pathTracerShader.setInt("u_is_rendering", 1);
                pathTracerShader.setInt("u_samples_per_pixel", 512); // 你可以改成可调参数
                pathTracerShader.setInt("u_frame_index", 0);         // 每次重新渲染从 0 开始

                glDispatchCompute((SCR_WIDTH + 15) / 16, (SCR_HEIGHT + 15) / 16, 1);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

                renderDone = true;
            }
        }
        else {
            // 预览模式（快速单帧）
            pathTracerShader.setInt("u_is_rendering", 0);
            pathTracerShader.setInt("u_samples_per_pixel", 1);
            pathTracerShader.setInt("u_frame_index", 0);

            glDispatchCompute((SCR_WIDTH + 15) / 16, (SCR_HEIGHT + 15) / 16, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            renderDone = false; // 每帧都刷新，保持预览动态
        }

        // 8. 显示
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glViewport(SIDEBAR_WIDTH, 0,
            SCR_WIDTH - SIDEBAR_WIDTH, SCR_HEIGHT);

        screenShader.use();
        glBindVertexArray(quadVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texOutput);
        screenShader.setInt("u_texOutput", 0); // <--- 一定要设置 uniform
        glDrawArrays(GL_TRIANGLES, 0, 6);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
