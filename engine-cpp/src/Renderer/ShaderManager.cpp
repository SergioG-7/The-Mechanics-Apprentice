#include "ShaderManager.h"

ShaderManager::ShaderManager(const std::string& vsPath, const std::string& fsPath) {
    m_shader = LoadShader(vsPath.c_str(), fsPath.c_str());
    if (m_shader.id == 0) {
        TraceLog(LOG_WARNING, "ShaderManager: fallo al compilar (%s, %s)",
                  vsPath.c_str(), fsPath.c_str());
    }
}

ShaderManager::~ShaderManager() {
    if (m_shader.id != 0) {
        UnloadShader(m_shader);
    }
}

int ShaderManager::GetLocation(const std::string& uniformName) const {
    return GetShaderLocation(m_shader, uniformName.c_str());
}
