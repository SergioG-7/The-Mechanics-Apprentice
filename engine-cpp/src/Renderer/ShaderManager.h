#pragma once
#include "raylib.h"
#include <string>

// Wrapper RAII sobre un Shader de raylib. Carga un par vertex/fragment GLSL
// y garantiza UnloadShader() al destruirse.
class ShaderManager {
public:
    ShaderManager(const std::string& vsPath, const std::string& fsPath);
    ~ShaderManager();

    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    Shader Get() const { return m_shader; }

private:
    Shader m_shader{};
};
