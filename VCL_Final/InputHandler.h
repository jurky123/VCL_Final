#pragma once
#include <GLFW/glfw3.h>
#include "camera.h"

// 输入处理（鼠标、键盘回调）
class InputHandler {
public:
    Camera* targetCamera = nullptr;
    bool isRenderingMode = false;

    static InputHandler* s_instance;

    InputHandler();
    ~InputHandler();

    void SetCamera(Camera* cam) { targetCamera = cam; }
    void SetRenderingMode(bool mode) { isRenderingMode = mode; }
    
    void SetupCallbacks(GLFWwindow* window);

private:
    float lastX = 600.0f, lastY = 400.0f;
    bool firstMouse = true;
    float deltaTime = 0.0f, lastFrame = 0.0f;

    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void MouseCallback(GLFWwindow* window, double xpos, double ypos);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    friend class ImGuiManager;
};