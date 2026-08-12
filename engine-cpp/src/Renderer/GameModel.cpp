#include "GameModel.h"

GameModel::GameModel(const std::string& path) {
    m_model = LoadModel(path.c_str());
    // raylib devuelve un Model con meshCount == 0 si falla, no lanza excepción.
    m_loaded = (m_model.meshCount > 0);
    if (!m_loaded) {
        TraceLog(LOG_WARNING, "GameModel: no se pudo cargar '%s'", path.c_str());
    }
}

GameModel::~GameModel() {
    if (m_loaded) {
        UnloadModel(m_model);
    }
}

GameModel::GameModel(GameModel&& other) noexcept
    : m_model(other.m_model), m_loaded(other.m_loaded) {
    other.m_loaded = false; // evita doble UnloadModel
    other.m_model = {};
}

GameModel& GameModel::operator=(GameModel&& other) noexcept {
    if (this != &other) {
        if (m_loaded) {
            UnloadModel(m_model);
        }
        m_model = other.m_model;
        m_loaded = other.m_loaded;
        other.m_loaded = false;
        other.m_model = {};
    }
    return *this;
}

void GameModel::Draw(Vector3 position, float scale, Color tint) const {
    if (m_loaded) {
        DrawModel(m_model, position, scale, tint);
    }
}

void GameModel::SetShader(Shader shader) {
    if (!m_loaded) return;
    for (int i = 0; i < m_model.materialCount; ++i) {
        m_model.materials[i].shader = shader;
    }
}
