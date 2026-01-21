#include "InputHandler.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include <iostream>

InputHandler* InputHandler::s_instance = nullptr;

InputHandler::InputHandler() {
    if (s_instance) {
        std::cerr << "[Warning] InputHandler already initialized\n";
    }
    s_instance = this;
}

InputHandler::~InputHandler() {
    if (s_instance == this) s_instance = nullptr;
}

static void ForwardToImGui_MouseButton(GLFWwindow* window, int button, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
}
static void ForwardToImGui_CursorPos(GLFWwindow* window, double xpos, double ypos) {
    ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
}
static void ForwardToImGui_Scroll(GLFWwindow* window, double xoffset, double yoffset) {
    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
}
static void ForwardToImGui_Key(GLFWwindow* window, int key, int scancode, int action, int mods) {
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
}
static void ForwardToImGui_Char(GLFWwindow* window, unsigned int c) {
    ImGui_ImplGlfw_CharCallback(window, c);
}

void InputHandler::SetupCallbacks(GLFWwindow* window) {
    // We forward events to ImGui ourselves (ImGui_ImplGlfw_InitForOpenGL was called with install_callbacks=false)
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    glfwSetCursorPosCallback(window, MouseCallback);
    glfwSetScrollCallback(window, ScrollCallback);
    glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int mods){
        ForwardToImGui_MouseButton(w, button, action, mods);
        // keep existing app behavior if needed (none here)
    });
    glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int scancode, int action, int mods){
        ForwardToImGui_Key(w, key, scancode, action, mods);
    });
    glfwSetCharCallback(window, [](GLFWwindow* w, unsigned int c){
        ForwardToImGui_Char(w, c);
    });
}

void InputHandler::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(300, 0, width - 300, height);
}

void InputHandler::MouseCallback(GLFWwindow* window, double xposIn, double yposIn) {
    // First forward to ImGui so ImGui can update IO state
    ForwardToImGui_CursorPos(window, xposIn, yposIn);

    if (!s_instance || !s_instance->targetCamera) return;
    if (ImGui::GetIO().WantCaptureMouse || s_instance->isRenderingMode) return;

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (s_instance->firstMouse) {
        s_instance->lastX = xpos;
        s_instance->lastY = ypos;
        s_instance->firstMouse = false;
    }

    float xoffset = xpos - s_instance->lastX;
    float yoffset = s_instance->lastY - ypos;
    s_instance->lastX = xpos;
    s_instance->lastY = ypos;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        s_instance->targetCamera->ProcessMouseMovement(xoffset, yoffset);
    }
}

void InputHandler::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    // Forward to ImGui first
    ForwardToImGui_Scroll(window, xoffset, yoffset);

    if (!s_instance || !s_instance->targetCamera) return;
    if (ImGui::GetIO().WantCaptureMouse || s_instance->isRenderingMode) return;

    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        s_instance->targetCamera->Zoom -= (float)yoffset * 2.0f;
        s_instance->targetCamera->Zoom = glm::clamp(s_instance->targetCamera->Zoom, 1.0f, 3000.0f);
    } else {
        if (yoffset > 0) s_instance->targetCamera->MovementSpeed *= 1.2f;
        else s_instance->targetCamera->MovementSpeed /= 1.2f;
        s_instance->targetCamera->MovementSpeed = glm::max(s_instance->targetCamera->MovementSpeed, 0.001f);
    }
}