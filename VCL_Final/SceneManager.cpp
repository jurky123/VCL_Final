#include "SceneManager.h"
#include "SceneRenderer.h"
#include <iostream>
#include <glm/glm.hpp>

SceneManager::SceneManager() {
    camera.Position = glm::vec3(0, 0, 5);
    camera.Forward = glm::vec3(0, 0, -1);
    camera.Zoom = 45.0f;
}

void SceneManager::LoadScene(const std::string& path) {
    bool success = false;
    std::string ext = path.substr(path.find_last_of(".") + 1);

    if (ext == "yaml") {
        success = loader.LoadSceneFromYAML(path);
    } else {
        loader.triangles.clear();
        success = loader.LoadModel(path);
    }

    if (success) {
        InitializeCamera(path);
        std::cout << "Successfully loaded: " << path << " (" << loader.triangles.size() 
                  << " triangles)\n";
    }

    for (size_t i = 0; i < loader.materials.size(); i++) {
        loader.materials[i].id = static_cast<int>(i);
    }

    CreateRasterMeshes();
    PrintSceneInfo();
}

void SceneManager::LoadSceneByIndex(int index) {
    if (index >= 0 && index < (int)scenePaths.size()) {
        LoadScene(scenePaths[index]);
    }
}

void SceneManager::CreateRasterMeshes() {
    sceneMeshes.clear();
    loader.CreateRasterMeshes(sceneMeshes);
}

void SceneManager::InitializeCamera(const std::string& path) {
    std::string ext = path.substr(path.find_last_of(".") + 1);

    if (ext == "yaml") {
        camera.Position = loader.initialEye;
        camera.Forward = glm::normalize(loader.initialTarget - loader.initialEye);
        camera.Zoom = loader.initialFovy;
        camera.Yaw = glm::degrees(atan2(camera.Forward.z, camera.Forward.x));
        camera.Pitch = glm::degrees(asin(camera.Forward.y));
    } else {
        camera.Position = glm::vec3(0, 0, 5);
        camera.Forward = glm::vec3(0, 0, -1);
        camera.Yaw = -90.0f;
        camera.Pitch = 0.0f;
        camera.Zoom = 45.0f;
    }
}

void SceneManager::SetupSmallPTScene(std::vector<CPU_Sphere>& outSpheres) {
    loader.triangles.clear();
    loader.materials.clear();
    sceneMeshes.clear();
    outSpheres.clear();

    auto addSphere = [&](float rad, glm::vec3 p, glm::vec3 e, glm::vec3 c, int type, float ior) {
        CPU_Sphere s{};
        s.pos = glm::vec4(p, rad);
        s.emission = glm::vec4(e, 0.0f);
        s.color = glm::vec4(c, 0.0f);
        s.type = type;
        s.ior = ior;
        s.pad0 = s.pad1 = 0;
        outSpheres.push_back(s);
    };

    addSphere(1e5f, glm::vec3(1e5f + 1, 40.8f, 81.6f), glm::vec3(0.0f),
              glm::vec3(.75f, .25f, .25f), 0, 1.0f);
    addSphere(1e5f, glm::vec3(-1e5f + 99, 40.8f, 81.6f), glm::vec3(0.0f),
              glm::vec3(.25f, .25f, .75f), 0, 1.0f);
    addSphere(1e5f, glm::vec3(50, 40.8f, 1e5f), glm::vec3(0.0f),
              glm::vec3(.75f, .75f, .75f), 0, 1.0f);
    addSphere(1e5f, glm::vec3(50, 40.8f, -1e5f + 170), glm::vec3(0.0f),
              glm::vec3(0.0f), 0, 1.0f);
    addSphere(1e5f, glm::vec3(50, 1e5f, 81.6f), glm::vec3(0.0f),
              glm::vec3(.75f, .75f, .75f), 0, 1.0f);
    addSphere(1e5f, glm::vec3(50, -1e5f + 81.6f, 81.6f), glm::vec3(0.0f),
              glm::vec3(.75f, .75f, .75f), 0, 1.0f);
    addSphere(16.5f, glm::vec3(27, 16.5f, 47), glm::vec3(0.0f),
              glm::vec3(.999f, .999f, .999f), 1, 1.0f);
    addSphere(16.5f, glm::vec3(73, 16.5f, 78), glm::vec3(0.0f),
              glm::vec3(.999f, .999f, .999f), 2, 1.5f);
    addSphere(600.0f, glm::vec3(50, 681.6f - .27f, 81.6f), glm::vec3(12.0f, 12.0f, 12.0f),
              glm::vec3(0.0f), 0, 1.0f);
}

void SceneManager::PrintSceneInfo() const {
    std::cout << "[Debug] triangles=" << loader.triangles.size()
              << ", materials=" << loader.materials.size()
              << ", meshes=" << sceneMeshes.size() << "\n";

    for (size_t i = 0; i < std::min<size_t>(20, loader.triangles.size()); ++i) {
        std::cout << "  Tri[" << i << "] mat_id=" << loader.triangles[i].material_id << "\n";
    }

    for (size_t i = 0; i < std::min<size_t>(10, loader.lights.size()); ++i) {
        const auto& l = loader.lights[i];
        std::cout << "  Light[" << i << "] pos=(" << l.position.x << "," << l.position.y
                  << "," << l.position.z << ")\n";
    }
}