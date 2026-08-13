#include "Application.h"
#include "../Combat/CombatSystem.h"
#include "../Combat/CollisionMath.h"
#include <iostream>

Application::Application(int width, int height, const std::string& title) {
    InitWindow(width, height, title.c_str());
    SetTargetFPS(60);
    InitAudioDevice();

    m_camera.position   = { 0.0f, 4.0f, 8.0f };
    m_camera.target     = { 0.0f, 1.0f, 0.0f };
    m_camera.up         = { 0.0f, 1.0f, 0.0f };
    m_camera.fovy       = 45.0f;
    m_camera.projection = CAMERA_PERSPECTIVE;

    m_bgm = LoadMusicStream("assets/audio/music/theme.ogg");
    if (m_bgm.frameCount > 0) {
        m_bgm.looping = true;
        SetMusicVolume(m_bgm, kMusicVolume);
    }

    m_toonShader = std::make_unique<ShaderManager>("shaders/toon.vs", "shaders/toon.fs");

    LoadLevel();
}

Application::~Application() {
    if (m_bgm.frameCount > 0) UnloadMusicStream(m_bgm);
    CloseAudioDevice();
    CloseWindow();
}

void Application::LoadLevel() {
    m_level = LevelLoader::LoadFromFile("assets/sample_level.json");

    if (m_level.player) {
        m_level.player->SetObstacles(&m_level.obstacles);
        m_level.player->SetShader(m_toonShader->Get());
    }
    for (auto& enemy : m_level.enemies) {
        enemy->SetObstacles(&m_level.obstacles);
        enemy->SetShader(m_toonShader->Get());
    }

    m_totalGears = static_cast<int>(m_level.gears.size());
    m_state = GameState::Gameplay;

    // Arranca (o reinicia desde el principio) la música al entrar en
    // partida; se para en GameOver/Victory para dar un respiro.
    if (m_bgm.frameCount > 0) PlayMusicStream(m_bgm);
}

void Application::Update(float dt) {
    if (m_bgm.frameCount > 0) UpdateMusicStream(m_bgm);

    if (m_level.player != nullptr) {
        Vector3 playerPos = m_level.player->GetPosition();

        m_camera.target = playerPos;

        // Offset isométrico: 15 unidades hacia arriba (Y) y 12 hacia atrás (Z)
        m_camera.position = Vector3{ playerPos.x, playerPos.y + 15.0f, playerPos.z + 12.0f };
    }

    switch (m_state) {
        case GameState::Gameplay:
            UpdateGameplay(dt);
            break;

        case GameState::GameOver:
        case GameState::Victory:
            // Pausado: solo se escucha la tecla de reinicio.
            if (IsKeyPressed(KEY_R)) {
                LoadLevel();
            }
            break;

        case GameState::Menu:
            break; // sin implementar todavía
    }
}

void Application::UpdateGameplay(float dt) {
    if (!m_level.player) return;

    m_level.player->Update(dt);

    for (auto& enemy : m_level.enemies) {
        enemy->NotifyPlayerPosition(m_level.player->GetPosition());
    }

    CombatSystem::ResolveMeleeAttack(*m_level.player, m_level.enemies);

    for (auto& enemy : m_level.enemies) {
        enemy->Update(dt);
    }

    CombatSystem::ResolveEnemyAttacks(*m_level.player, m_level.enemies);

    // Recolección de engranajes: chequeo AABB simple Player vs cada Gear.
    for (auto it = m_level.gears.begin(); it != m_level.gears.end(); ) {
        if (CollisionMath::AABBIntersects(m_level.player->GetBoundingBox(), (*it)->GetBoundingBox())) {
            std::cout << "[Gameplay] Engranaje recogido!" << std::endl;
            it = m_level.gears.erase(it);
        } else {
            ++it;
        }
    }

    // --- Condiciones de fin de partida ---
    if (!m_level.player->IsAlive()) {
        TraceLog(LOG_INFO, "Application: GAME OVER");
        m_state = GameState::GameOver;
        if (m_bgm.frameCount > 0) StopMusicStream(m_bgm);
        return;
    }

    if (m_level.door && m_level.gears.empty() &&
        CollisionMath::AABBIntersects(m_level.player->GetBoundingBox(), m_level.door->GetBoundingBox())) {
        TraceLog(LOG_INFO, "Application: VICTORY");
        m_state = GameState::Victory;
        if (m_bgm.frameCount > 0) StopMusicStream(m_bgm);
    }
}

void Application::DrawHud() const {
    if (!m_level.player) return;

    // --- Barra de HP del Player ---
    constexpr int barX = 10, barY = 40, barWidth = 200, barHeight = 20;
    float hpRatio = m_level.player->GetHP() / m_level.player->GetMaxHP();
    DrawRectangle(barX, barY, barWidth, barHeight, DARKGRAY);
    DrawRectangle(barX, barY, static_cast<int>(barWidth * hpRatio), barHeight, RED);
    DrawRectangleLines(barX, barY, barWidth, barHeight, BLACK);

    // --- Engranajes recolectados / total ---
    int collected = m_totalGears - static_cast<int>(m_level.gears.size());
    DrawText(TextFormat("Engranajes: %d / %d", collected, m_totalGears), barX, barY + barHeight + 10, 20, BLACK);

    // --- Barra de HP flotante sobre cada Enemy dañado ---
    for (auto& enemy : m_level.enemies) {
        if (!enemy->IsAlive()) continue;               // un zombie derrotado no necesita barra
        if (enemy->GetHP() >= enemy->GetMaxHP()) continue; // solo tras el primer golpe, para no saturar

        Vector3 worldPos = enemy->GetPosition();
        worldPos.y += 1.0f; // encima del cubo, no sobre su centro
        Vector2 screenPos = GetWorldToScreen(worldPos, m_camera);

        constexpr int enemyBarWidth = 40, enemyBarHeight = 6;
        int ex = static_cast<int>(screenPos.x) - enemyBarWidth / 2;
        int ey = static_cast<int>(screenPos.y);
        float enemyHpRatio = enemy->GetHP() / enemy->GetMaxHP();

        DrawRectangle(ex, ey, enemyBarWidth, enemyBarHeight, DARKGRAY);
        DrawRectangle(ex, ey, static_cast<int>(enemyBarWidth * enemyHpRatio), enemyBarHeight, RED);
    }
}

void Application::DrawGroundGrid() const {
    // Cuadrícula holográfica estilo Tron en vez del DrawGrid gris por
    // defecto de raylib -- raylib no define CYAN, así que se usa SKYBLUE
    // (mismo sustituto ya usado para el contorno de Obstacle).
    constexpr int slices = 20;
    constexpr float spacing = 1.0f;
    constexpr float halfSize = (slices * spacing) / 2.0f;
    Color gridColor = Fade(SKYBLUE, 0.3f);

    for (int i = -slices / 2; i <= slices / 2; i++) {
        float offset = i * spacing;
        DrawLine3D(Vector3{ offset, 0.0f, -halfSize }, Vector3{ offset, 0.0f, halfSize }, gridColor);
        DrawLine3D(Vector3{ -halfSize, 0.0f, offset }, Vector3{ halfSize, 0.0f, offset }, gridColor);
    }
}

void Application::DrawCenteredOverlay(const char* title, Color titleColor, const char* subtitle) const {
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    DrawRectangle(0, 0, screenW, screenH, Color{ 0, 0, 0, 150 });

    int titleSize = 60;
    int titleWidth = MeasureText(title, titleSize);
    DrawText(title, (screenW - titleWidth) / 2, screenH / 2 - 40, titleSize, titleColor);

    int subSize = 20;
    int subWidth = MeasureText(subtitle, subSize);
    DrawText(subtitle, (screenW - subWidth) / 2, screenH / 2 + 30, subSize, RAYWHITE);
}

void Application::Draw() {
    BeginDrawing();
    ClearBackground(Color{ 30, 30, 35, 255 });

    BeginMode3D(m_camera);
    DrawGroundGrid();

    if (m_level.player) m_level.player->Draw();
    for (auto& enemy : m_level.enemies) enemy->Draw();
    for (auto& obstacle : m_level.obstacles) obstacle->Draw();
    for (auto& gear : m_level.gears) gear->Draw();
    if (m_level.door) m_level.door->Draw();
    EndMode3D();

    DrawHud();

    switch (m_state) {
        case GameState::GameOver:
            DrawCenteredOverlay("GAME OVER", RED, "Pulsa R para reintentar");
            break;
        case GameState::Victory:
            DrawCenteredOverlay("VICTORIA", GREEN, "Pulsa R para jugar de nuevo");
            break;
        default:
            break;
    }

    DrawFPS(10, 10);
    EndDrawing();
}

void Application::Run() {
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        Update(dt);
        Draw();
    }
}
