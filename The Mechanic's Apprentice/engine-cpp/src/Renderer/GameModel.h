#pragma once
#include "raylib.h"
#include <string>

// Wrapper RAII sobre Model de raylib.
// Es dueño de los recursos de GPU (meshes, materiales, texturas) y garantiza
// que UnloadModel() se llame exactamente una vez, incluso con retornos tempranos.
class GameModel {
public:
    explicit GameModel(const std::string& path);
    ~GameModel();

    // No copiable: Model contiene handles de GPU, copiarlo provocaría doble free.
    GameModel(const GameModel&) = delete;
    GameModel& operator=(const GameModel&) = delete;

    // Movible: transfiere la propiedad y deja el origen en un estado "vacío" seguro.
    GameModel(GameModel&& other) noexcept;
    GameModel& operator=(GameModel&& other) noexcept;

    void Draw(Vector3 position, float scale, Color tint) const;
    void SetShader(Shader shader);

    bool IsValid() const { return m_loaded; }

private:
    Model m_model{};
    bool m_loaded = false;
};
