#pragma once
#include <string>
#include <vector>
#include "ModelLoader.h"
#include "camera.h"
#include "Sphere.h"

// 场景管理和加载
class SceneManager {
public:
    ModelLoader loader;
    std::vector<Mesh> sceneMeshes;
    Camera camera;

    std::vector<std::string> scenePaths = {
        "models/slime.obj",
        "models/cube.obj",
        "models/cornell_box/cornell_box.yaml",
        "models/breakfast_room/breakfast_room.yaml"
    };

    SceneManager();

    void LoadScene(const std::string& path);
    void LoadSceneByIndex(int index);
    void CreateRasterMeshes();
    void SetupSmallPTScene(std::vector<CPU_Sphere>& outSpheres);
    void PrintSceneInfo() const;

private:
    void InitializeCamera(const std::string& path);
};