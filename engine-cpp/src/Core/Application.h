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

    // Juice de combate, disparado desde UpdateGameplay al conectar un golpe.
    void AddCameraShake(float duration, float intensity);
    void TriggerHitStop(float duration);

private:
    // --- AppState::MainMenu / Options ---
    void UpdateMenu();
    void DrawMenu() const;

    // Abre el editor de niveles de nivel-editor-csharp como proceso aparte
    // -- WinForms no se puede incrustar en la ventana de raylib, así que es
    // una ventana de Windows independiente, no un panel embebido. Ruta
    // relativa al cwd del juego (build/Debug), solo válida en la máquina de
    // desarrollo con ambos proyectos compilados en Debug. Dos caminos hasta
    // aquí: el atajo oculto F12 (cualquier pantalla de menú) y el botón
    // "Editor de Niveles" del menú principal, anclado en la esquina superior
    // izquierda (ver MenuAction::OpenLevelEditor) -- proyecto de portfolio,
    // así que el acceso visible tiene sentido aunque solo funcione en esta
    // máquina. El idioma activo se pasa como argumento de línea de comandos
    // (ver Program.cs del editor) para que abra ya localizado igual que el
    // juego, sin depender de qué idioma se dejó la última vez dentro del
    // propio editor.
    void LaunchLevelEditor() const;

    // --- AppState::StoryMode / EndlessMode / Paused ---
    // Todo lo que corre siempre en una pantalla de juego (cámara, música,
    // timers de juice, partículas) más, si la partida sigue viva, delega en
    // UpdateActiveMatch. Si no sigue viva (GameOver/Victory), gestiona el
    // input de pausa (reintentar, siguiente nivel, volver al menú).
    void UpdateGameplay(float dt);
    void DrawGameplay() const;
    void UpdateActiveMatch(float dt);
    void HandleGameplayPauseInput();

    // GameState::WaitingToStart: el Player NO se actualiza (ni movimiento ni
    // dash) y enemigos/trampas/spawners tampoco, hasta que se pulsa Atacar
    // -- entonces pasa a GameState::Gameplay y el siguiente frame ya entra
    // por UpdateActiveMatch.
    void UpdateWaitingToStart();

    // ESC durante una partida viva (ver UpdateGameplay) -- distinto de
    // HandleGameplayPauseInput, que gestiona la pantalla de GameOver/Victory,
    // no una pausa real.
    void UpdatePauseMenu();

    // Vuelca m_saveManager.Data().bgmVolume/sfxVolume a MusicController y
    // AudioSettings, y reaplica el de SFX a las entidades YA cargadas (el
    // slider de Opciones puede tocarse en pausa, a media partida, sin
    // recargar el nivel). Se llama cada frame que la pantalla de Opciones
    // está abierta -- barato, y así funciona igual se abra desde el menú
    // principal o desde la pausa.
    void ApplyLiveAudioSettings();

    void DrawGroundGrid() const;

    // Recalcula desde cero el bonus de velocidad que el aura de cada Buffer
    // (ver EnemyBehavior::Buffer) aplica a los zombis a su alrededor. Vive
    // aquí y no en Enemy porque un Enemy no conoce a los demás -- igual que
    // el AoE del Kamikaze lo aplica CombatSystem desde fuera.
    void ApplyBufferAuras();

    // Tirada de drop al morir un enemigo en Modo Historia: power-up
    // (ponderado por rareza) o botiquín, o nada. El Modo Infinito tiene su
    // propia economía (engranaje = puntuación) y no pasa por aquí.
    void RollStoryModeDrop(Vector3 position);

    UiContext BuildUiContext() const { return UiContext{ m_localization }; }

    // Carga (o recarga) el nivel indicado desde disco y deja la partida en
    // Gameplay. Si el archivo no existe (fin del Modo Historia, o un
    // endless.json que aún no se ha creado), vuelve a AppState::MainMenu en
    // vez de dejar la partida a medio construir.
    void LoadLevel(const std::string& path);

    void StartStoryMode(int level);
    void StartEndlessMode();
    void AdvanceToNextStoryLevel();
    static std::string BuildStoryLevelPath(int level);

    // Escanea assets/audio/music/ y devuelve la ruta del primer .ogg/.wav/
    // .mp3 que encuentre, o "" si no hay ninguno -- antes era un nombre
    // hardcodeado ("theme.ogg") que se quedó apuntando a un archivo ya
    // borrado en cuanto el usuario lo reemplazó por otro con distinto
    // nombre/extensión (MusicController cargaba en silencio, sin sonar).
    // Un escaneo no depende de qué nombre tenga el archivo real, solo de
    // que exista UNO en la carpeta esperada.
    static std::string FindMusicFile();

    AppState m_appState = AppState::MainMenu;
    bool m_quitRequested = false;
    MenuScreen m_menuScreen;
    HudRenderer m_hud;

    // A qué AppState (StoryMode/EndlessMode) volver al pulsar "Continuar" en
    // la pausa -- AppState::Paused no distingue por sí solo cuál de los dos
    // modos estaba corriendo.
    AppState m_pausedFromState = AppState::StoryMode;

    // A qué AppState volver al pulsar "Volver" en Opciones: MainMenu si se
    // abrió desde el menú principal, Paused si se abrió desde la pausa.
    // MenuScreen no conoce AppState, así que esta decisión vive aquí.
    AppState m_optionsReturnTo = AppState::MainMenu;

    // Persistencia (save_data.json) e idioma. SaveManager se construye
    // primero (lee CurrentLanguage de disco) para que LocalizationManager
    // pueda arrancar ya en el idioma correcto, no siempre en español.
    SaveManager m_saveManager;
    LocalizationManager m_localization;

    Camera3D m_camera{};
    GameState m_matchState = GameState::Gameplay;
    std::unique_ptr<MusicController> m_music;

    std::unique_ptr<ShaderManager> m_toonShader;

    LevelData m_level;
    std::string m_currentLevelPath; // último path pasado a LoadLevel(); lo reusa el reintento con 'R'
    int m_totalGears = 0; // fijado al cargar el nivel; m_level.gears.size() baja al recogerlos

    // Modo Historia: nivel actual (1-indexado); assets/data/level_<N>.json.
    int m_currentLevel = 1;

    // Cuántos level_<N>.json existen de verdad en assets/data. Es el tope de
    // maxLevelUnlocked: sin él, superar el último nivel "desbloqueaba" uno
    // más que no existe, y el selector le pintaba un botón que solo servía
    // para rebotar al menú. Añadir un nivel nuevo implica subir esto.
    static constexpr int kStoryLevelCount = 10;

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

    // Charcos de lodo que van dejando los Trapper. Igual que m_projectiles:
    // sin dueño propio, nacen de Enemy::ConsumePendingPuddle y caducan solos
    // en CombatSystem::UpdateMudPuddles.
    std::vector<MudPuddle> m_puddles;
};
