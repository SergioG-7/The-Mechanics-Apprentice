#include "Application.h"
#include "../Combat/CombatSystem.h"
#include "../Combat/CollisionMath.h"
#include "../Entities/Gear.h"
#include "../Entities/HealthKit.h"
#include "../Entities/ExplosiveBarrel.h"
#include <algorithm>
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

    // Arranca en el menú (m_appState = AppState::MainMenu por defecto): no
    // hay LoadLevel aquí, se dispara al elegir Historia/Infinito.
}

Application::~Application() {
    if (m_bgm.frameCount > 0) UnloadMusicStream(m_bgm);
    CloseAudioDevice();
    CloseWindow();
}

void Application::LoadLevel(const std::string& path) {
    m_currentLevelPath = path;
    m_level = LevelLoader::LoadFromFile(path);

    if (!m_level.player) {
        // Sin jugador = archivo inexistente o mal formado (LevelLoader ya
        // avisó por log). Es el caso normal de "Modo Historia sin más
        // niveles": en vez de dejar la partida a medio construir, se vuelve
        // al menú principal.
        TraceLog(LOG_WARNING, "Application: nivel '%s' no disponible, volviendo al menu", path.c_str());
        m_appState = AppState::MainMenu;
        return;
    }

    m_level.player->SetObstacles(&m_level.obstacles);
    m_level.player->SetShader(m_toonShader->Get());
    for (auto& enemy : m_level.enemies) {
        enemy->SetObstacles(&m_level.obstacles);
        enemy->SetShader(m_toonShader->Get());
    }

    m_totalGears = static_cast<int>(m_level.gears.size());
    m_matchState = GameState::Gameplay;

    // Spawners data-driven, leídos del propio nivel (ver LevelLoader.cpp).
    // Se recrean aquí, no solo la primera vez, para que un reintento con 'R'
    // también limpie sus timers y su cupo de enemigos vivos.
    m_spawners.clear();
    for (const SpawnerData& data : m_level.spawners) {
        m_spawners.emplace_back(data.position, data.enemyType, data.interval, data.maxEnemies,
                                 &m_level.obstacles, m_toonShader->Get());
    }

    // Arranca (o reinicia desde el principio) la música al entrar en
    // partida; se para en GameOver/Victory para dar un respiro.
    if (m_bgm.frameCount > 0) PlayMusicStream(m_bgm);
}

std::string Application::BuildStoryLevelPath(int level) {
    return "assets/data/level_" + std::to_string(level) + ".json";
}

void Application::StartStoryMode() {
    m_appState = AppState::StoryMode;
    m_currentLevel = 1;
    LoadLevel(BuildStoryLevelPath(m_currentLevel));
}

void Application::StartEndlessMode() {
    m_appState = AppState::EndlessMode;
    m_endlessDirector.Reset();
    LoadLevel("assets/data/endless.json");
}

void Application::AdvanceToNextStoryLevel() {
    m_currentLevel++;
    LoadLevel(BuildStoryLevelPath(m_currentLevel));
}

// --- Menú ---

void Application::UpdateMenu() {
    MenuAction action = (m_appState == AppState::Options)
        ? m_menuScreen.UpdateOptions()
        : m_menuScreen.UpdateMainMenu();

    switch (action) {
        case MenuAction::StartStory:     StartStoryMode();   break;
        case MenuAction::StartEndless:   StartEndlessMode(); break;
        case MenuAction::OpenOptions:    m_appState = AppState::Options; break;
        case MenuAction::BackToMainMenu: m_appState = AppState::MainMenu; break;
        case MenuAction::Quit:           m_quitRequested = true; break;
        case MenuAction::None:           break;
    }
}

void Application::DrawMenu() const {
    BeginDrawing();
    ClearBackground(Color{ 20, 20, 25, 255 });

    if (m_appState == AppState::Options) {
        m_menuScreen.DrawOptions();
    } else {
        m_menuScreen.DrawMainMenu();
    }

    EndDrawing();
}

// --- Gameplay (StoryMode / EndlessMode) ---

void Application::UpdateGameplay(float dt) {
    if (m_bgm.frameCount > 0) UpdateMusicStream(m_bgm);

    if (m_shakeTimer > 0.0f) m_shakeTimer -= dt;
    if (m_hitStopTimer > 0.0f) m_hitStopTimer -= dt;
    m_particles.Update(dt);

    if (m_level.player != nullptr) {
        Vector3 playerPos = m_level.player->GetPosition();

        m_camera.target = playerPos;

        // Offset isométrico: 15 unidades hacia arriba (Y) y 12 hacia atrás (Z)
        m_camera.position = Vector3{ playerPos.x, playerPos.y + 15.0f, playerPos.z + 12.0f };

        // Shake sumado ENCIMA de la posición ya calculada, nunca la sustituye
        // -- si escribiera m_camera.position aparte, pelearía con el offset
        // isométrico de arriba en vez de mezclarse con él.
        if (m_shakeTimer > 0.0f) {
            float offsetX = (static_cast<float>(GetRandomValue(-100, 100)) / 100.0f) * m_shakeIntensity;
            float offsetZ = (static_cast<float>(GetRandomValue(-100, 100)) / 100.0f) * m_shakeIntensity;
            m_camera.position.x += offsetX;
            m_camera.position.z += offsetZ;
        }
    }

    if (m_matchState != GameState::Gameplay) {
        HandleGameplayPauseInput();
        return;
    }

    UpdateActiveMatch(dt);
}

void Application::HandleGameplayPauseInput() {
    if (!IsKeyPressed(KEY_R)) return;

    // Infinito no se reintenta in situ: el punto era justo terminar y
    // enseñar la puntuación, así que 'R' devuelve al menú.
    if (m_matchState == GameState::GameOver && m_appState == AppState::EndlessMode) {
        m_appState = AppState::MainMenu;
        return;
    }

    // Historia + Victoria: 'R' continúa al siguiente nivel en vez de repetir
    // el que se acaba de superar.
    if (m_matchState == GameState::Victory && m_appState == AppState::StoryMode) {
        AdvanceToNextStoryLevel();
        return;
    }

    // Cualquier otro caso (típicamente Historia + GameOver): reintentar tal
    // cual el último nivel cargado.
    LoadLevel(m_currentLevelPath);
}

void Application::UpdateActiveMatch(float dt) {
    if (!m_level.player) return;

    // Hit-stop: Player/Enemy ven dt = 0 (FSM y animación congeladas) mientras
    // el timer sigue corriendo con el dt real de Application::UpdateGameplay.
    float entityDt = (m_hitStopTimer > 0.0f) ? 0.0f : dt;

    m_level.player->Update(entityDt);

    for (auto& enemy : m_level.enemies) {
        enemy->NotifyPlayerPosition(m_level.player->GetPosition());
    }

    constexpr float kHitStopDuration = 0.05f;
    constexpr float kHitShakeDuration = 0.1f;
    constexpr float kHitShakeIntensity = 0.15f;
    constexpr int kMinHitParticles = 5;
    constexpr int kMaxHitParticles = 10;

    if (auto hit = CombatSystem::ResolveMeleeAttack(*m_level.player, m_level.enemies)) {
        TriggerHitStop(kHitStopDuration);
        AddCameraShake(kHitShakeDuration, kHitShakeIntensity);
        m_particles.Emit(hit->impactPoint, GetRandomValue(kMinHitParticles, kMaxHitParticles));

        // Solo en Infinito: cada baja deja un engranaje (95%) o, más raro, un
        // botiquín (5%) -- así el modo se sostiene solo, sin depender de los
        // objetos fijos de un nivel.
        if (m_appState == AppState::EndlessMode && hit->hitEnemy && !hit->hitEnemy->IsAlive()) {
            constexpr int kHealthKitDropChancePercent = 5;
            if (GetRandomValue(1, 100) <= kHealthKitDropChancePercent) {
                m_level.healthKits.push_back(std::make_unique<HealthKit>(hit->hitEnemy->GetPosition()));
            } else {
                m_level.gears.push_back(std::make_unique<Gear>(hit->hitEnemy->GetPosition()));
            }
        }
    } else if (auto barrelHit = CombatSystem::ResolveMeleeAttackOnBarrels(*m_level.player, m_level.barrels)) {
        // Mismo juice que golpear a un enemigo; la propia explosión (si el
        // barril llega a 0 HP) se resuelve más abajo, tras el update.
        TriggerHitStop(kHitStopDuration);
        AddCameraShake(kHitShakeDuration, kHitShakeIntensity);
        m_particles.Emit(*barrelHit, GetRandomValue(kMinHitParticles, kMaxHitParticles));
    }

    for (auto& enemy : m_level.enemies) {
        enemy->Update(entityDt);

        Projectile projectile;
        if (enemy->ConsumePendingProjectile(projectile)) {
            m_projectiles.push_back(projectile);
        }

        if (enemy->ConsumeExplosionTrigger()) {
            CombatSystem::ApplyAreaDamage(enemy->GetPosition(), Enemy::kExplodeRadius, enemy->GetExplosionDamage(),
                                           *m_level.player, m_level.enemies);
            m_particles.Emit(enemy->GetPosition(), GetRandomValue(15, 25));
        }
    }

    CombatSystem::ResolveEnemyAttacks(*m_level.player, m_level.enemies);
    CombatSystem::UpdateProjectiles(entityDt, m_projectiles, *m_level.player);

    for (auto& spawner : m_spawners) {
        spawner.Update(entityDt, m_level.enemies);
    }

    if (m_appState == AppState::EndlessMode) {
        m_endlessDirector.Update(entityDt, m_spawners);
    }

    // Barriles: aplica el AoE compartido con el Kamikaze en cuanto uno llega
    // a 0 HP y lo retira -- HasExploded() solo puede pasar a true una vez,
    // así que no hace falta un flag de "ya procesado".
    for (auto& barrel : m_level.barrels) {
        if (!barrel->HasExploded()) continue;
        CombatSystem::ApplyAreaDamage(barrel->GetPosition(), ExplosiveBarrel::kExplosionRadius,
                                       ExplosiveBarrel::kExplosionDamage, *m_level.player, m_level.enemies);
        m_particles.Emit(barrel->GetPosition(), GetRandomValue(15, 25));
    }
    m_level.barrels.erase(
        std::remove_if(m_level.barrels.begin(), m_level.barrels.end(),
                        [](const std::unique_ptr<ExplosiveBarrel>& b) { return b->HasExploded(); }),
        m_level.barrels.end());

    // Corpse cleanup: saca del vector a los enemigos que ya terminaron su
    // fade-out (ver Enemy::UpdateDead). Sus punteros ya no están en ningún
    // Spawner::m_spawnedEnemies -- Spawner::Update purga por IsAlive() cada
    // frame, mucho antes de que un cadáver llegue a este punto.
    m_level.enemies.erase(
        std::remove_if(m_level.enemies.begin(), m_level.enemies.end(),
                        [](const std::unique_ptr<Enemy>& e) { return e->IsPendingDestruction(); }),
        m_level.enemies.end());

    // Recolección de engranajes: chequeo AABB simple Player vs cada Gear.
    for (auto it = m_level.gears.begin(); it != m_level.gears.end(); ) {
        if (CollisionMath::AABBIntersects(m_level.player->GetBoundingBox(), (*it)->GetBoundingBox())) {
            std::cout << "[Gameplay] Engranaje recogido!" << std::endl;
            if (m_appState == AppState::EndlessMode) m_endlessDirector.OnGearCollected();
            it = m_level.gears.erase(it);
        } else {
            ++it;
        }
    }

    // Recolección de botiquines: cura al Player y desaparece.
    for (auto it = m_level.healthKits.begin(); it != m_level.healthKits.end(); ) {
        if (CollisionMath::AABBIntersects(m_level.player->GetBoundingBox(), (*it)->GetBoundingBox())) {
            std::cout << "[Gameplay] Botiquin recogido!" << std::endl;
            m_level.player->Heal((*it)->GetHealAmount());
            it = m_level.healthKits.erase(it);
        } else {
            ++it;
        }
    }

    // --- Condiciones de fin de partida ---
    if (!m_level.player->IsAlive()) {
        TraceLog(LOG_INFO, "Application: GAME OVER");
        m_matchState = GameState::GameOver;
        if (m_bgm.frameCount > 0) StopMusicStream(m_bgm);
        return;
    }

    // Infinito no tiene puerta de victoria: solo termina al morir el
    // jugador (ver HandleGameplayPauseInput).
    if (m_appState == AppState::StoryMode && m_level.door && m_level.gears.empty() &&
        CollisionMath::AABBIntersects(m_level.player->GetBoundingBox(), m_level.door->GetBoundingBox())) {
        TraceLog(LOG_INFO, "Application: VICTORY");
        m_matchState = GameState::Victory;
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

    // --- Engranajes: en Infinito es la puntuación (sin total fijo); en
    // Historia, recolectados / total del nivel. ---
    if (m_appState == AppState::EndlessMode) {
        DrawText(TextFormat("Engranajes: %d", m_endlessDirector.GetScore()), barX, barY + barHeight + 10, 20, BLACK);
    } else {
        int collected = m_totalGears - static_cast<int>(m_level.gears.size());
        DrawText(TextFormat("Engranajes: %d / %d", collected, m_totalGears), barX, barY + barHeight + 10, 20, BLACK);
    }

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

void Application::DrawGameplay() const {
    BeginDrawing();
    ClearBackground(Color{ 30, 30, 35, 255 });

    BeginMode3D(m_camera);
    DrawGroundGrid();

    if (m_level.player) m_level.player->Draw();
    for (auto& enemy : m_level.enemies) enemy->Draw();
    for (auto& obstacle : m_level.obstacles) obstacle->Draw();
    for (auto& gear : m_level.gears) gear->Draw();
    for (auto& healthKit : m_level.healthKits) healthKit->Draw();
    for (auto& barrel : m_level.barrels) barrel->Draw();
    for (const Projectile& projectile : m_projectiles) DrawSphere(projectile.position, Projectile::kRadius, YELLOW);
    if (m_level.door) m_level.door->Draw();
    m_particles.Draw();
    EndMode3D();

    DrawHud();

    switch (m_matchState) {
        case GameState::GameOver:
            DrawCenteredOverlay("GAME OVER", RED,
                (m_appState == AppState::EndlessMode) ? "Pulsa R para volver al menu" : "Pulsa R para reintentar");
            break;
        case GameState::Victory:
            DrawCenteredOverlay("VICTORIA", GREEN, "Pulsa R para continuar");
            break;
        default:
            break;
    }

    DrawFPS(10, 10);
    EndDrawing();
}

void Application::AddCameraShake(float duration, float intensity) {
    m_shakeTimer = duration;
    m_shakeIntensity = intensity;
}

void Application::TriggerHitStop(float duration) {
    // max, no asignación directa: un segundo golpe durante un hit-stop ya
    // activo no debe acortarlo.
    m_hitStopTimer = std::max(m_hitStopTimer, duration);
}

void Application::Run() {
    while (!WindowShouldClose() && !m_quitRequested) {
        switch (m_appState) {
            case AppState::MainMenu:
            case AppState::Options:
                UpdateMenu();
                DrawMenu();
                break;

            case AppState::StoryMode:
            case AppState::EndlessMode:
                UpdateGameplay(GetFrameTime());
                DrawGameplay();
                break;
        }
    }
}
