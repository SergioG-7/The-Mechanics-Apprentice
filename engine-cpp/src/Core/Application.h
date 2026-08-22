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
#include "../Renderer/CameraRig.h"
#include "../VFX/ParticleSystem.h"
#include "../Entities/Spawner.h"
#include "../Entities/MudPuddle.h"
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

    // Congela brevemente el juego al conectar un golpe, para dar sensación de impacto.
    void TriggerHitStop(float duration);

private:
    // --- AppState::MainMenu / Options ---
    void UpdateMenu();
    void DrawMenu() const;

    // Abre el editor de niveles como un proceso de Windows aparte.
    void LaunchLevelEditor() const;

    // --- AppState::StoryMode / EndlessMode / Paused ---
    void UpdateGameplay(float dt);
    void DrawGameplay() const;
    void UpdateActiveMatch(float dt);
    void HandleGameplayPauseInput();

    // Deja quieto al jugador y al resto del nivel hasta que se pulsa Atacar para empezar.
    void UpdateWaitingToStart();

    // Menú de pausa (ESC durante una partida en curso).
    void UpdatePauseMenu();

    // Aplica los volúmenes de música y efectos actuales a lo que ya está sonando.
    void ApplyLiveAudioSettings();

    void DrawGroundGrid() const;

    // Recoge todo lo que el jugador esté tocando: engranajes, power-ups, botiquines.
    void CollectPickups();

    // Aplica el daño en área de todos los barriles que hayan explotado este frame.
    void ResolveBarrelExplosions();

    // Corta el screen shake y el hit-stop en seco, al cambiar de pantalla.
    void ClearTransientEffects();

    UiContext BuildUiContext() const { return UiContext{ m_localization }; }

    // Carga el nivel indicado y deja la partida lista para jugar.
    void LoadLevel(const std::string& path);

    void StartStoryMode(int level);
    void StartEndlessMode();
    void AdvanceToNextStoryLevel();
    static std::string BuildStoryLevelPath(int level);

    // Busca el primer archivo de música en assets/audio/music/.
    static std::string FindMusicFile();

    AppState m_appState = AppState::MainMenu;
    bool m_quitRequested = false;
    MenuScreen m_menuScreen;
    HudRenderer m_hud;

    // A qué AppState volver al pulsar "Continuar" en la pausa.
    AppState m_pausedFromState = AppState::StoryMode;

    // A qué AppState volver al cerrar Opciones (menú principal o pausa).
    AppState m_optionsReturnTo = AppState::MainMenu;

    // A qué AppState volver al cerrar la Guía.
    AppState m_guideReturnTo = AppState::MainMenu;

    SaveManager m_saveManager;
    LocalizationManager m_localization;

    // Cámara isométrica con screen shake.
    CameraRig m_camera;
    GameState m_matchState = GameState::Gameplay;
    std::unique_ptr<MusicController> m_music;

    std::unique_ptr<ShaderManager> m_toonShader;

    LevelData m_level;
    std::string m_currentLevelPath; // para volver a cargar el mismo nivel al reintentar
    int m_totalGears = 0; // engranajes totales del nivel, fijado al cargarlo

    // Modo Historia: nivel actual (1-indexado).
    int m_currentLevel = 1;

    // Cuántos niveles de Modo Historia existen en total.
    static constexpr int kStoryLevelCount = 15;

    // Modo Infinito: dificultad, drop de engranajes y puntuación.
    EndlessDirector m_endlessDirector;

    // Mientras está activo, el juego se congela brevemente para dar sensación de impacto.
    CountdownTimer m_hitStopTimer;

    ParticleSystem m_particles;

    // Spawners del nivel actual.
    std::vector<Spawner> m_spawners;

    // Proyectiles disparados por los enemigos Spitter.
    std::vector<Projectile> m_projectiles;

    // Charcos de lodo dejados por los enemigos Trapper.
    std::vector<MudPuddle> m_puddles;
};
