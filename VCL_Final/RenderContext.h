#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// 渲染窗口和 OpenGL 上下文管理
class RenderContext {
public:
    static constexpr unsigned int DEFAULT_WIDTH = 1200;
    static constexpr unsigned int DEFAULT_HEIGHT = 800;
    static constexpr int SIDEBAR_WIDTH = 300;

    GLFWwindow* window = nullptr;
    unsigned int width, height;
    bool isInitialized = false;

    RenderContext(unsigned int w = DEFAULT_WIDTH, unsigned int h = DEFAULT_HEIGHT);
    ~RenderContext();

    bool Initialize();
    bool IsRunning() const;
    void SwapBuffers();
    void PollEvents();
    void Shutdown();

private:
    void SetupGLFWHints();
    bool CreateWindow();
};