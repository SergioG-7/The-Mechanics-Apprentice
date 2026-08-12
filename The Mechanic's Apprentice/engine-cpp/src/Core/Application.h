#pragma once
#include "raylib.h"
#include "GameState.h"
#include "../Renderer/GameModel.h"
#include "../Renderer/ShaderManager.h"
#include "../IO/LevelLoader.h"
#include <memory>
#include <string>

class Application {
public:
    Application(int width, int height, const std::string& title);
    ~Application();

    void Run();

private:
    void Update(float dt);
    void Draw();

    // Carga (o recarga) el nivel desde disco y deja Application en
    // Gameplay. La usan tanto el constructor como el reinicio con 'R'.
    void LoadLevel();

    void UpdateGameplay(float dt);
    void DrawHud() const;
    void DrawCenteredOverlay(const char* title, Color titleColor, const char* subtitle) const;

    Camera3D m_camera{};
    GameState m_state = GameState::Gameplay;

    std::unique_ptr<GameModel> m_testModel;
    std::unique_ptr<ShaderManager> m_toonShader;

    LevelData m_level;
    int m_totalGears = 0; // fijado al cargar el nivel; m_level.gears.size() baja al recogerlos
};
