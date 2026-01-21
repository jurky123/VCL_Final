#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

class Shader {
public:
    unsigned int ID;

    Shader() : ID(0) {}

    // 构造函数 1：用于普通的渲染管线 (Vertex + Fragment)
    Shader(const char* vertexPath, const char* fragmentPath) {
        std::string vertexCode = readFile(vertexPath);
        std::string fragmentCode = readFile(fragmentPath);

        unsigned int vertex = compileShader(vertexCode.c_str(), GL_VERTEX_SHADER, "VERTEX");
        unsigned int fragment = compileShader(fragmentCode.c_str(), GL_FRAGMENT_SHADER, "FRAGMENT");

        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        linkProgram(ID);

        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    // 构造函数 2：专门用于 Compute Shader
    Shader(const char* computePath) {
        std::string computeCode = readFile(computePath);
        unsigned int compute = compileShader(computeCode.c_str(), GL_COMPUTE_SHADER, "COMPUTE");

        ID = glCreateProgram();
        glAttachShader(ID, compute);
        linkProgram(ID);

        glDeleteShader(compute);
    }

    // 激活着色器
    void use() const { glUseProgram(ID); }

    // --- Uniform 工具函数 ---
    void setBool(const std::string& name, bool value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
    }
    void setInt(const std::string& name, int value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }
    void setFloat(const std::string& name, float value) const {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }
    void setMat4(const std::string& name, const glm::mat4& mat) const {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }
    void setVec3(const std::string& name, const glm::vec3& value) const {
        glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
    }

private:
    // 读取文件到字符串
    std::string readFile(const char* path) {
        std::string code;
        std::ifstream file;
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        try {
            file.open(path);
            std::stringstream stream;
            stream << file.rdbuf();
            file.close();
            code = stream.str();
        }
        catch (std::ifstream::failure& e) {
            std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << path << std::endl;
        }
        return code;
    }

    // 编译单个着色器
    unsigned int compileShader(const char* code, GLenum type, std::string name) {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &code, NULL);
        glCompileShader(shader);
        checkCompileErrors(shader, name);
        return shader;
    }

    // 链接程序
    void linkProgram(unsigned int programID) {
        glLinkProgram(programID);
        checkCompileErrors(programID, "PROGRAM");
    }

    // 错误检查
    void checkCompileErrors(unsigned int shader, std::string type) {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
        else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
    }
};

#endif