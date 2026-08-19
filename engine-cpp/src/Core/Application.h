#pragma once
#include "raylib.h"
#include "AppState.h"
#include "GameState.h"
#include "EndlessDirector.h"
#include "SaveManager.h"
#include "LocalizationManager.h"
#include "CountdownTimer.h"
#include "../Audio/MusicController.h"
#include "../IO/LevelLoader.h"
#include "../Renderer/ShaderManager.h"
#include "../VFX/ParticleSystem.h"
#include "../Entities/Spawner.h"
#include "../Combat/Projectile.h"
#include "../UI/MenuScreen.h"
#include "../UI/HudRenderer.h"
#include "../UI/UiContext.h"
#include <memory>
#include <string>
#include <vector>

class Application {
public:
    Application(int width, int height, const std::string& title);
    ~Application();

    void Run();

    // Juice de combate, disparado desde UpdateGameplay al conectar un golpe.
    void AddCameraShake(float duration, float intensity);
    void TriggerHitStop(float duration);

private:
    // --- AppState::MainMenu / Options ---
    void UpdateMenu();
    void DrawMenu() const;

    // Atajo de desarrollo oculto (F12, cualquier pantalla de menú): abre el
    // editor de niveles de nivel-editor-csharp como proceso aparte -- WinForms
    // no se puede incrustar en la ventana de raylib, así que es una ventana
    // de Windows independiente, no un panel embebido. Ruta relativa al cwd
    // del juego (build/Debug), solo válida en la máquina de desarrollo con
    // ambos proyectos compilados en Debug -- no tiene sentido en una build
    // para repartir, por eso no hay ningún botón visible para esto.
    void LaunchLevelEditor() const;

    // --- AppState::StoryMode / EndlessMode ---
    // Todo lo que corre siempre en una pantalla de juego (cámara, música,
    // timers de juice, partículas) más, si la partida sigue viva, delega en
    // UpdateActiveMatch. Si no sigue viva (GameOver/Victory), gestiona el
    // input de pausa (reintentar, siguiente nivel, volver al menú).
    void UpdateGameplay(float dt);
    void DrawGameplay() const;
    void UpdateActiveMatch(float dt);
    void HandleGameplayPauseInput();

    void DrawGroundGrid() const;

    // Concatena el texto de LocalizationManager (los tres idiomas a la vez)
    // y lo pasa por LoadCodepoints -- el conjunto de glifos que necesita
    // LoadFontEx para que el japonés (y los acentos del español) se vean
    // bien sea cual sea el idioma activo al arrancar o tras cambiarlo.
    void LoadUiFont();
    UiContext BuildUiContext() const { return UiContext{ m_font, m_localization }; }

    // Carga (o recarga) el nivel indicado desde disco y deja la partida en
    // Gameplay. Si el archivo no existe (fin del Modo Historia, o un
    // endless.json que aún no se ha creado), vuelve a AppState::MainMenu en
    // vez de dejar la partida a medio construir.
    void LoadLevel(const std::string& path);

    void StartStoryMode(int level);
    void StartEndlessMode();
    void AdvanceToNextStoryLevel();
    static std::string BuildStoryLevelPath(int level);

    AppState m_appState = AppState::MainMenu;
    bool m_quitRequested = false;
    MenuScreen m_menuScreen;
    HudRenderer m_hud;

    // Persistencia (save_data.json) e idioma. SaveManager se construye
    // primero (lee CurrentLanguage de disco) para que LocalizationManager
    // pueda arrancar ya en el idioma correcto, no siempre en español.
    SaveManager m_saveManager;
    LocalizationManager m_localization;
    Font m_font{};

    Camera3D m_camera{};
    GameState m_matchState = GameState::Gameplay;
    std::unique_ptr<MusicController> m_music;
    static constexpr float kMusicVolume = 0.03f; // 0.0 (silencio) - 1.0 (volumen original del archivo)

    std::unique_ptr<ShaderManager> m_toonShader;

    LevelData m_level;
    std::string m_currentLevelPath; // último path pasado a LoadLevel(); lo reusa el reintento con 'R'
    int m_totalGears = 0; // fijado al cargar el nivel; m_level.gears.size() baja al recogerlos

    // Modo Historia: nivel actual (1-indexado); assets/data/level_<N>.json.
    int m_currentLevel = 1;

    // Modo Infinito: dificultad, drop de engranajes y puntuación -- ver
    // EndlessDirector, que vive fuera de Application a propósito.
    EndlessDirector m_endlessDirector;

    // Screen shake: offset aleatorio sumado sobre la posición de cámara ya
    // calculada a partir del jugador (Application::UpdateGameplay), nunca
    // escrito por otro lado.
    CountdownTimer m_shakeTimer;
    float m_shakeIntensity = 0.0f;

    // Hit-stop: mientras esté activo, Player/Enemy reciben dt = 0 en su
    // Update (ver UpdateActiveMatch), pero el timer se descuenta con el dt
    // real de cada frame para no congelarse a sí mismo.
    CountdownTimer m_hitStopTimer;

    ParticleSystem m_particles;

    // Se construyen en LoadLevel() a partir de m_level.spawners (ver
    // LevelLoader.cpp / LevelData.h) y se recrean en cada llamada, incluido
    // el reintento con 'R', así que su timer y su cupo de vivos siempre
    // arrancan limpios.
    std::vector<Spawner> m_spawners;

    // Proyectiles del Spitter, movidos y comprobados contra el Player en
    // CombatSystem::UpdateProjectiles. No tienen dueño propio: nacen cuando
    // Enemy::ConsumePendingProjectile devuelve uno y mueren al impactar o
    // expirar (erase-remove, dentro de esa misma función).
    std::vector<Projectile> m_projectiles;
};
