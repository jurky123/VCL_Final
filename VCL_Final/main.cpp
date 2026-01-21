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
#include "GPU_Material.h"
// --- 配置与全局变量 ---
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 800;
const int SIDEBAR_WIDTH = 300;
GLuint ssboMaterials = 0;
GLuint ssboLights = 0;
GLuint ssboSpheres = 0;
bool ptTrianglesDirty = true;
Camera camera;
float lastX = (SCR_WIDTH - SIDEBAR_WIDTH) / 2.0f + SIDEBAR_WIDTH;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
int frameIndex = 0;
bool isRendering = false; // 初始为预览模式
int pathTracerMaxBounces = 1; // 默认最多1 次反射
int pathTracerSamplesPerPixel = 1; // 默认每像素采样次数

// 场景与 SSBO
unsigned int ssboTriangles;
ModelLoader loader;
std::vector<Mesh> sceneMeshes;
// CPU-side sphere representation used by UI and SSBO uploads
struct CPU_Sphere {
    glm::vec4 pos; // xyz = center, w = radius
    glm::vec4 emission; // rgb
    glm::vec4 color; // rgb
    int type; //0=DIFF,1=SPEC,2=REFR
    float ior;
    int pad0;
    int pad1;
};
std::vector<CPU_Sphere> cpuSpheres;
//------------debug-----------
std::vector<std::string> scenePaths = {
    "models/slime.obj",
    "models/cube.obj",
    "models/cornell_box/cornell_box.yaml",
    "models/breakfast_room/breakfast_room.yaml"
};
int currentSceneIndex = 0;
// 全局变量
GLuint texArray = 0; // 最终的纹理数组

void CreateMaterialTextureArray(std::vector<Material>& materials)
{
    // ---------------- Step 1: 分配层索引（从 1 开始，0 表示无纹理） ----------------
    int layerIndex = 1;
    for (auto& m : materials) {
        if (!m.diffuseTexPath.empty()) {
            m.diffuseTexID = layerIndex++; // 有纹理的材质分配层，从 1 开始
        }
        else {
            m.diffuseTexID = 0;           // 0 表示无纹理
        }
    }

    int texCount = layerIndex - 1;
    if (texCount == 0) return; // 没有纹理，直接返回

    // ---------------- Step 2: 找出最大宽高并加载所有图片 ----------------
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
        int layer = m.diffuseTexID - 1; // 存储在 loadedData 的 0-based 索引
        loadedData[layer] = data;

        if (w > maxWidth) maxWidth = w;
        if (h > maxHeight) maxHeight = h;
    }

    // ---------------- Step 3: 创建 Texture2DArray ----------------
    glGenTextures(1, &texArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texArray);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, maxWidth, maxHeight, texCount);

    // ---------------- Step 4: 上传每一层 ----------------
    for (auto& m : materials) {
        if (m.diffuseTexID == 0) continue;

        int layer = m.diffuseTexID - 1;
        unsigned char* data = loadedData[layer];
        if (!data) continue;

        int w, h, n;
        stbi_info(m.diffuseTexPath.c_str(), &w, &h, &n);

        // 填充到最大尺寸
        std::vector<unsigned char> layerData(maxWidth * maxHeight * 4, 255); // 默认白色
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                for (int c = 0; c < 4; ++c) {
                    layerData[(y * maxWidth + x) * 4 + c] = data[(y * w + x) * 4 + c];
                }
            }
        }

        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
            0, 0, layer,
            maxWidth, maxHeight, 1,
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
    for (size_t i = 0; i < loader.materials.size(); i++) {
        loader.materials[i].id = static_cast<int>(i);
    }
    ptTrianglesDirty = true;
    std::vector<std::string> texPaths;


}
void UploadMaterialsSSBO() {
    std::vector<GPU_Material> gpuMats;
    gpuMats.reserve(loader.materials.size());
    for (auto& m : loader.materials) {
        GPU_Material gm{};
        gm.diffuse = glm::vec4(m.diffuse, 0.0f);
        gm.specular = glm::vec4(m.specular, 0.0f);
        gm.emission = m.emission;
        gm.metallic = m.metallic;
        gm.shiness = m.shiness;
        // 修正：把 layer 索引上传，而不是 GL 纹理对象名
        gm.diffuseTexID = static_cast<int>(m.diffuseTexID);
        gpuMats.push_back(gm);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboMaterials);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
        gpuMats.size() * sizeof(GPU_Material),
        gpuMats.data(),
        GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboMaterials);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);


    std::cout << "[SSBO] Uploaded " << gpuMats.size() << " materials\n";
}
void UploadLightsSSBO() {
    struct GPU_Light_CPU {
        int type;
        int pad0;
        int pad1;
        int pad2;
        glm::vec4 position;
        glm::vec4 direction;
        glm::vec4 intensity;
    };

    std::vector<GPU_Light_CPU> gpuLights;
    gpuLights.reserve(loader.lights.size());
    for (auto& l : loader.lights) {
        GPU_Light_CPU gl;
        gl.type = (l.type == Light::Point) ? 0 : 1;
        gl.pad0 = gl.pad1 = gl.pad2 = 0;
        gl.position = glm::vec4(l.position, 1.0f);
        gl.direction = glm::vec4(glm::normalize(l.direction), 0.0f);
        gl.intensity = glm::vec4(l.intensity, 0.0f);
        gpuLights.push_back(gl);
    }

    if (ssboLights == 0) glGenBuffers(1, &ssboLights);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboLights);
    if (!gpuLights.empty()) {
        glBufferData(GL_SHADER_STORAGE_BUFFER, gpuLights.size() * sizeof(GPU_Light_CPU), gpuLights.data(), GL_STATIC_DRAW);
    }
    else {
        glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_STATIC_DRAW);
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, ssboLights);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    std::cout << "[SSBO] Uploaded " << gpuLights.size() << " lights\n";
}

void UploadSpheresSSBO() {
    if (ssboSpheres ==0) glGenBuffers(1, &ssboSpheres);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboSpheres);
    if (!cpuSpheres.empty()) {
        glBufferData(GL_SHADER_STORAGE_BUFFER, cpuSpheres.size() * sizeof(CPU_Sphere), cpuSpheres.data(), GL_STATIC_DRAW);
    } else {
        glBufferData(GL_SHADER_STORAGE_BUFFER,0, nullptr, GL_STATIC_DRAW);
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,5, ssboSpheres);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);

    std::cout << "[SSBO] Uploaded " << cpuSpheres.size() << " spheres\n";
}
void PrintSceneInfo() {
    // Debug: 打印载入后的信息，帮助定位材质/mesh 分组问题
    std::cout << "[Debug] loader.triangles=" << loader.triangles.size()
        << ", loader.materials=" << loader.materials.size()
        << ", sceneMeshes=" << sceneMeshes.size() << std::endl;
    for (size_t i = 0; i < std::min<size_t>(20, loader.triangles.size()); ++i) {
        std::cout << "Tri[" << i << "] mat_id=" << loader.triangles[i].material_id << "\n";
    }
    for (size_t i = 0; i < sceneMeshes.size(); ++i) {
        std::cout << "Mesh[" << i << "] material_index=" << sceneMeshes[i].material_index
            << ", diffuse=(" << sceneMeshes[i].material.diffuse.r << "," << sceneMeshes[i].material.diffuse.g << "," << sceneMeshes[i].material.diffuse.b << ")\n";
    }

    // 打印灯光信息
    for (size_t i = 0; i < loader.lights.size(); ++i) {
        const auto& l = loader.lights[i];
        std::cout << "Light[" << i << "] pos=(" << l.position.x << "," << l.position.y << "," << l.position.z << "), "
            << "intensity=(" << l.intensity.r << "," << l.intensity.g << "," << l.intensity.b << ")\n";
    }

    // 打印球体信息 (from cpuSpheres)
    for (size_t i =0; i < cpuSpheres.size(); ++i) {
        const auto& s = cpuSpheres[i];
        std::cout << "Sphere[" << i << "] pos=(" << s.pos.x << "," << s.pos.y << "," << s.pos.z << "), "
            << "radius=" << s.pos.w << "\n";
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
        if (camera.Zoom > 3000.0f) camera.Zoom = 3000.0f;
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



    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Path Tracer Lab - Offline Mode", NULL, NULL);
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

    glGenBuffers(1, &ssboTriangles);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboTriangles);

    glGenBuffers(1, &ssboMaterials);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboMaterials);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // 4. 加载场景
    LoadScene(scenePaths[currentSceneIndex]);

    loader.CreateRasterMeshes(sceneMeshes);
    PrintSceneInfo();
    //6. 上传 Material 数据
    CreateMaterialTextureArray(loader.materials);

    UploadMaterialsSSBO();
    // 上传灯光数据到 SSBO
    UploadLightsSSBO();
    UploadSpheresSSBO();


    // 5. 上传 Triangle 数据
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboTriangles);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
        loader.triangles.size() * sizeof(Triangle),
        loader.triangles.data(),
        GL_STATIC_DRAW);




    // 7. 全屏 Quad
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

    // 8. 输出纹理
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

    // 9. 渲染循环
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // --- ImGui 新帧 ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- ImGui 界面 ---
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2((float)SIDEBAR_WIDTH, (float)SCR_HEIGHT));
        ImGui::Begin("Scene Control", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Triangles: %d", (int)loader.triangles.size());
        ImGui::Separator();

        if (ImGui::Checkbox("Enable Path Tracing", &isRendering)) {
            renderDone = false;
            // reset accumulation
            frameIndex = 0;
            // clear output texture to zeros to start fresh
            if (texOutput) {
                glBindTexture(GL_TEXTURE_2D, texOutput);
                std::vector<float> zeros(SCR_WIDTH * SCR_HEIGHT * 4, 0.0f);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_RGBA, GL_FLOAT, zeros.data());
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }

        if (isRendering) {
            ImGui::TextColored(ImVec4(1, 0.5, 0, 1), "Mode: PATH TRACING");
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
        const char* items[] = { "Slime", "Cube", "Cornell Box","Breakfast_room" };
        if (ImGui::Combo("Select Scene", &currentSceneIndex, items, 4)) {
            LoadScene(scenePaths[currentSceneIndex]);
            loader.CreateRasterMeshes(sceneMeshes);
            PrintSceneInfo();

            //6. 上传 Material 数据
            CreateMaterialTextureArray(loader.materials);

            UploadMaterialsSSBO();
            UploadLightsSSBO();
            UploadSpheresSSBO();
            renderDone = false;
            frameIndex = 0; // reset accumulation when scene changes
            if (texOutput) {
                glBindTexture(GL_TEXTURE_2D, texOutput);
                std::vector<float> zeros(SCR_WIDTH * SCR_HEIGHT * 4, 0.0f);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_RGBA, GL_FLOAT, zeros.data());
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }
        ImGui::SliderFloat("FOV", &camera.Zoom, 1.0f, 3000.0f);
        ImGui::Text("Camera Speed: %.3f", camera.MovementSpeed);

        // Path tracer controls
        ImGui::SliderInt("Max Bounces", &pathTracerMaxBounces, 0, 8);
        ImGui::SliderInt("Samples Per Pixel", &pathTracerSamplesPerPixel, 1, 10000);

        if (ImGui::Button("Reset View")) {
            LoadScene(scenePaths[currentSceneIndex]);
            loader.CreateRasterMeshes(sceneMeshes);
            CreateMaterialTextureArray(loader.materials);

            UploadMaterialsSSBO();
            UploadLightsSSBO();
            UploadSpheresSSBO();
            renderDone = false;
            frameIndex = 0;
            if (texOutput) {
                glBindTexture(GL_TEXTURE_2D, texOutput);
                std::vector<float> zeros(SCR_WIDTH * SCR_HEIGHT * 4, 0.0f);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_RGBA, GL_FLOAT, zeros.data());
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }

        // Hard-coded SmallPT scene
        if (ImGui::Button("Load smallpt scene")) {
            // clear triangle geometry and CPU materials
            loader.triangles.clear();
            loader.materials.clear();
            sceneMeshes.clear();

            cpuSpheres.clear();
            auto addS = [&](float rad, glm::vec3 p, glm::vec3 e, glm::vec3 c, int type, float ior) {
                CPU_Sphere s{};
                s.pos = glm::vec4(p, rad);
                s.emission = glm::vec4(e, 0.0f);
                s.color = glm::vec4(c, 0.0f);
                s.type = type;
                s.ior = ior;
                s.pad0 = s.pad1 = 0;
                cpuSpheres.push_back(s);
                };
            // smallpt default scene
            addS(1e5f, glm::vec3(1e5f + 1, 40.8f, 81.6f), glm::vec3(0.0f), glm::vec3(.75f, .25f, .25f), 0, 1.0f); // left
            addS(1e5f, glm::vec3(-1e5f + 99, 40.8f, 81.6f), glm::vec3(0.0f), glm::vec3(.25f, .25f, .75f), 0, 1.0f); // right
            addS(1e5f, glm::vec3(50, 40.8f, 1e5f), glm::vec3(0.0f), glm::vec3(.75f, .75f, .75f), 0, 1.0f); // back
            addS(1e5f, glm::vec3(50, 40.8f, -1e5f + 170), glm::vec3(0.0f), glm::vec3(0.0f), 0, 1.0f); // front
            addS(1e5f, glm::vec3(50, 1e5f, 81.6f), glm::vec3(0.0f), glm::vec3(.75f, .75f, .75f), 0, 1.0f); // bottom
            addS(1e5f, glm::vec3(50, -1e5f + 81.6f, 81.6f), glm::vec3(0.0f), glm::vec3(.75f, .75f, .75f), 0, 1.0f); // top
            addS(16.5f, glm::vec3(27, 16.5f, 47), glm::vec3(0.0f), glm::vec3(.999f, .999f, .999f), 1, 1.0f); // mirror
            addS(16.5f, glm::vec3(73, 16.5f, 78), glm::vec3(0.0f), glm::vec3(.999f, .999f, .999f), 2, 1.5f); // glass
            addS(600.0f, glm::vec3(50, 681.6f - .27f, 81.6f), glm::vec3(12.0f, 12.0f, 12.0f), glm::vec3(0.0f), 0, 1.0f); // light

            UploadSpheresSSBO();

            // clear triangle SSBO on GPU
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboTriangles);
            glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_STATIC_DRAW);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

            // reset accumulation and clear output
            frameIndex = 0; renderDone = false;
            if (texOutput) {
                glBindTexture(GL_TEXTURE_2D, texOutput);
                std::vector<float> zeros(SCR_WIDTH * SCR_HEIGHT * 4, 0.0f);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_RGBA, GL_FLOAT, zeros.data());
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }
        ImGui::End();

        // --- 路径追踪 ---
        pathTracerShader.use();
        pathTracerShader.setInt("u_triangle_count", (int)loader.triangles.size());
        pathTracerShader.setInt("u_material_count", (int)loader.materials.size());

        float aspect = float(SCR_WIDTH - SIDEBAR_WIDTH) / float(SCR_HEIGHT);
        pathTracerShader.setMat4("u_inv_view", camera.GetInverseViewMatrix());
        pathTracerShader.setMat4("u_inv_proj", camera.GetInverseProjectionMatrix(aspect));

        // Do not dispatch compute here — dispatch will be done once when entering the
        // path-tracing branch below. This avoids running the heavy compute shader
        // every frame and freezing the UI.

        // 绘制到屏幕
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (isRendering) {
            glViewport(SIDEBAR_WIDTH, 0, SCR_WIDTH - SIDEBAR_WIDTH, SCR_HEIGHT);

            // --- Step 0: 生成 Path Tracing Triangles（只在场景更新时做一次） ---
            static std::vector<Triangle> ptTriangles;

            if (ptTrianglesDirty) {
                ptTriangles.clear();

                for (auto& mesh : sceneMeshes) {
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

                        tri.material_id = mesh.material_index; // ? 使用索引

                        ptTriangles.push_back(tri);
                    }
                    ptTrianglesDirty = false;
                }
                for (int i = 0; i < 10 && i < ptTriangles.size(); ++i)
                    std::cout << "Tri " << i << " material_id=" << ptTriangles[i].material_id << "\n";
                // 上传到 GPU
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboTriangles);
                glBufferData(GL_SHADER_STORAGE_BUFFER,
                    ptTriangles.size() * sizeof(Triangle),
                    ptTriangles.data(),
                    GL_STATIC_DRAW);
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

                ptTrianglesDirty = false; // 只上传一次
            }
            if (texArray != 0) {
                glActiveTexture(GL_TEXTURE3);
                glBindTexture(GL_TEXTURE_2D_ARRAY, texArray);
            }
            // --- Step1: Dispatch Compute Shader once ---
            if (!renderDone) {
                // Progressive rendering: dispatch one sample per frame to avoid long blocking compute calls
                pathTracerShader.use();

                // bind texture array sampler to unit3
                pathTracerShader.setInt("u_texArray", 3);
                // upload and bind lights SSBO and count
                pathTracerShader.setInt("u_light_count", (int)loader.lights.size());
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, ssboLights);

                pathTracerShader.setInt("u_triangle_count", (int)ptTriangles.size());
                pathTracerShader.setInt("u_material_count", (int)loader.materials.size());
                pathTracerShader.setInt("u_frame_index", frameIndex);
                // perform1 sample per dispatch to keep each compute call short
                pathTracerShader.setInt("u_samples_per_pixel", pathTracerSamplesPerPixel);
                pathTracerShader.setInt("u_max_bounces", pathTracerMaxBounces);

                float aspect = float(SCR_WIDTH - SIDEBAR_WIDTH) / float(SCR_HEIGHT);
                pathTracerShader.setMat4("u_inv_view", camera.GetInverseViewMatrix());
                pathTracerShader.setMat4("u_inv_proj", camera.GetInverseProjectionMatrix(aspect));

                glBindImageTexture(0, texOutput, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboTriangles);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboMaterials);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, ssboSpheres);

                GLuint groupX = (SCR_WIDTH + 15) / 16;
                GLuint groupY = (SCR_HEIGHT + 15) / 16;
                glDispatchCompute(groupX, groupY, 1);

                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

                // increment frame index (accumulated samples)
                frameIndex++;
                if (frameIndex >= pathTracerSamplesPerPixel) {
                    renderDone = true;
                }
            }
            // --- Step2: 渲染到屏幕 ---
            screenShader.use();
            glBindVertexArray(quadVAO);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texOutput);
            screenShader.setInt("u_texOutput", 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        else {
            // --- 光栅化渲染 ---
            glEnable(GL_DEPTH_TEST);

            rasterShader.use();
            rasterShader.setMat4("view", camera.GetViewMatrix());
            rasterShader.setMat4("proj", glm::perspective(glm::radians(camera.Zoom),
                float(SCR_WIDTH - SIDEBAR_WIDTH) / SCR_HEIGHT, 0.1f, 10000.0f));
            rasterShader.setVec3("u_cam_pos", camera.Position);

            // 设置环境光默认值，确保未使用贴图时也有基础亮度
            rasterShader.setVec3("u_ambient", glm::vec3(0.1f));

            // 灯光传递
            int light_count = std::min((int)loader.lights.size(), 16);
            rasterShader.setInt("u_light_count", light_count);
            for (int i = 0; i < light_count; i++) {
                rasterShader.setVec3("u_light_positions[" + std::to_string(i) + "]", loader.lights[i].position);
                rasterShader.setVec3("u_light_intensities[" + std::to_string(i) + "]", loader.lights[i].intensity);
            }

            // 绘制 Mesh
            for (auto& mesh : sceneMeshes) {
                //传材质属性
                rasterShader.setVec3("diffuse", mesh.material.diffuse);
                rasterShader.setVec3("specular", mesh.material.specular);
                rasterShader.setFloat("shiness", mesh.material.shiness);
                rasterShader.setFloat("emission", mesh.material.emission);

                //传贴图信息
                if (mesh.material.diffuseTex) {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, mesh.material.diffuseTex);
                    rasterShader.setInt("u_diffuseTex", 0);
                    rasterShader.setInt("hasDiffuseTex", 1); // 必须加上
                }
                else {
                    rasterShader.setInt("hasDiffuseTex", 0);
                }

                glBindVertexArray(mesh.VAO);
                glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
            }


            glBindVertexArray(0);
            glDisable(GL_DEPTH_TEST);
        }

        // --- ImGui 渲染 ---
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 10. Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();

    return 0;
}

