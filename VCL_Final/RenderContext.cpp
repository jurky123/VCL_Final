#include "RenderContext.h"
#include <iostream>

RenderContext::RenderContext(unsigned int w, unsigned int h)
    : width(w), height(h) {}

RenderContext::~RenderContext() {
    Shutdown();
}

bool RenderContext::Initialize() {
    glfwInit();
    SetupGLFWHints();
    
    if (!CreateWindow()) {
        std::cerr << "[Error] Failed to create GLFW window\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "[Error] Failed to load OpenGL functions\n";
        return false;
    }

    isInitialized = true;
    return true;
}

void RenderContext::SetupGLFWHints() {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
}

bool RenderContext::CreateWindow() {
    window = glfwCreateWindow(width, height, "Path Tracer Lab - Offline Mode", nullptr, nullptr);
    return window != nullptr;
}

bool RenderContext::IsRunning() const {
    return window && !glfwWindowShouldClose(window);
}

void RenderContext::SwapBuffers() {
    glfwSwapBuffers(window);
}

void RenderContext::PollEvents() {
    glfwPollEvents();
}

void RenderContext::Shutdown() {
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
    isInitialized = false;
}