#include "Application.h"
#include "AudioSettings.h"
#include "DropTable.h"
#include "../Combat/CombatSystem.h"
#include "../Combat/CollisionMath.h"
#include "../Entities/Gear.h"
#include "../Entities/HealthKit.h"
#include "../Entities/ExplosiveBarrel.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>

Application::Application(int width, int height, const std::string& title) {
    InitWindow(width, height, title.c_str());
    SetTargetFPS(60);
    InitAudioDevice();

    // Explícito, no confiado al valor por defecto de miniaudio: el volumen
    // real que oye el jugador es SetMasterVolume × MusicController/SFX, así
    // que si esto no fuera 1.0 ningún slider de Opciones podría llegar
    // nunca al 100% real por mucho que la UI marcara 100%.
    SetMasterVolume(1.0f);

    // Sin esto, ESC cierra la ventana de golpe (comportamiento por defecto
    // de raylib) en vez de abrir el menú de pausa -- ver UpdateGameplay.
    SetExitKey(KEY_NULL);

    // La cámara se configura sola en su constructor (ver CameraRig).

    // m_saveManager ya se ha construido (lee save_data.json en su propio
    // constructor, antes que nada aquí) -- el volumen y el idioma arrancan
    // directamente en lo guardado, no siempre en los valores por defecto.
    AudioSettings::SetSfxVolume(m_saveManager.Data().sfxVolume);

    // Construido en el cuerpo, no en la lista de inicialización: necesita
    // InitAudioDevice() ya llamado (arriba), igual que m_toonShader necesita
    // la ventana/contexto GL ya creados por InitWindow(). La ruta se
    // escanea en disco (FindMusicFile), no un nombre fijo -- ver el
    // comentario de esa función.
    m_music = std::make_unique<MusicController>(FindMusicFile(), m_saveManager.Data().bgmVolume);

    // m_menuScreen es un miembro por VALOR (no unique_ptr), así que ya se
    // construyó antes de InitAudioDevice() -- su SFX de clic se carga aquí,
    // explícitamente, ahora que el dispositivo de audio ya está listo.
    m_menuScreen.LoadSfx();

    m_toonShader = std::make_unique<ShaderManager>("shaders/toon.vs", "shaders/toon.fs");

    // m_localization es igualmente un miembro por VALOR construido antes de
    // InitWindow() -- LoadFonts() necesita el contexto GL ya creado, así
    // que también se llama aquí, explícitamente, después de InitWindow().
    m_localization.LoadAll(m_saveManager.Data().currentLanguage);
    m_localization.LoadFonts();

    // Arranca en el menú (m_appState = AppState::MainMenu por defecto): no
    // hay LoadLevel aquí, se dispara al elegir Historia/Infinito.
}

Application::~Application() {
    // Los recursos de GPU/audio (modelos, texturas y sonidos de m_level,
    // el shader, la música, la fuente) tienen que liberarse ANTES de cerrar
    // el contexto que los sostiene. Dejarlo a la destrucción automática de
    // miembros habría sido incorrecto: esa destrucción ocurre DESPUÉS de
    // que termine el cuerpo de este destructor, es decir, después de
    // CloseAudioDevice()/CloseWindow() si esas dos llamadas fueran lo único
    // aquí -- liberando GPU/audio contra un contexto ya cerrado. Se liberan
    // aquí a mano, en el orden correcto, antes de cerrar nada.
    m_level = LevelData{};
    m_toonShader.reset();
    m_music.reset();
    m_localization.UnloadFonts();

    CloseAudioDevice();
    CloseWindow();
}

std::string Application::FindMusicFile() {
    constexpr const char* kMusicDir = "assets/audio/music";
    // Filtro con la sintaxis de IsFileExtension (".ext1;.ext2;..."), no una
    // lista de comas -- ver el comentario de LoadDirectoryFilesEx en raylib.h.
    FilePathList files = LoadDirectoryFilesEx(kMusicDir, ".ogg;.wav;.mp3", false);

    std::string path;
    if (files.count > 0) {
        path = files.paths[0]; // el primero que haya: solo se espera un tema de fondo
    } else {
        TraceLog(LOG_WARNING, "Application: no se encontró ningún archivo de música en '%s'", kMusicDir);
    }

    UnloadDirectoryFiles(files);
    return path;
}

void Application::LoadLevel(const std::string& path) {
    m_currentLevelPath = path;

    // Estado transitorio de la partida anterior (o del intento anterior, si
    // esto es un reintento con 'R'): se limpia ANTES de tocar m_level, no
    // después, para no dejar una ventana en la que m_spawners siga
    // apuntando (por Spawner::m_spawnedEnemies, Enemy* no propietarios) a
    // los Enemy que la reasignación de m_level está a punto de destruir.
    // Sin esto: un proyectil de Spitter en vuelo, o un screen shake/hit-stop
    // activo justo al cruzar la puerta, sobrevivían al cambio de nivel.
    m_spawners.clear();
    m_projectiles.clear();
    m_puddles.clear();
    ClearTransientEffects();

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

    // Arranca en espera, no en juego: el jugador puede moverse desde ya,
    // pero enemigos/trampas/spawners no se activan hasta que se pulsa
    // Atacar (ver Application::UpdateWaitingToStart) -- un respiro antes de
    // que empiece a moverse nada, en cada carga de nivel Y en cada
    // reintento.
    m_matchState = GameState::WaitingToStart;

    // Spawners data-driven, leídos del propio nivel (ver LevelLoader.cpp) --
    // ya se limpiaron arriba, antes de reasignar m_level.
    for (const SpawnerData& data : m_level.spawners) {
        m_spawners.emplace_back(data.position, data.enemyType, data.interval, data.maxEnemies,
                                 &m_level.obstacles, m_toonShader->Get(), data.weightedTypes);
    }

    // Arranca (o reinicia desde el principio) la música al entrar en
    // partida; se para en GameOver/Victory para dar un respiro.
    m_music->Play();
}

std::string Application::BuildStoryLevelPath(int level) {
    return "assets/data/level_" + std::to_string(level) + ".json";
}

void Application::StartStoryMode(int level) {
    m_appState = AppState::StoryMode;
    m_currentLevel = level;
    LoadLevel(BuildStoryLevelPath(m_currentLevel));
}

void Application::StartEndlessMode() {
    m_appState = AppState::EndlessMode;
    m_endlessDirector.Reset();
    LoadLevel("assets/data/endless.json");
}

void Application::AdvanceToNextStoryLevel() {
    m_currentLevel++;

    // Se acabó la historia: al SELECTOR DE NIVELES, no al menú principal --
    // quien acaba de terminar el último nivel casi siempre quiere rejugar
    // alguno, y el menú principal le obligaba a entrar otra vez en "Modo
    // Historia" para llegar al mismo sitio. maxLevelUnlocked no se toca:
    // marcar como desbloqueado un nivel que no existe en disco le pintaba al
    // selector un botón que solo rebotaba al menú (vía el "nivel no
    // disponible" de LoadLevel).
    if (m_currentLevel > kStoryLevelCount) {
        m_saveManager.Save();
        m_music->Stop();
        m_appState = AppState::LevelSelect;
        return;
    }

    if (m_currentLevel > m_saveManager.Data().maxLevelUnlocked) {
        m_saveManager.Data().maxLevelUnlocked = m_currentLevel;
    }
    LoadLevel(BuildStoryLevelPath(m_currentLevel));
}

// --- Menú ---

void Application::LaunchLevelEditor() const {
    // std::system en vez de ShellExecute/CreateProcess a propósito: la API
    // Win32 (windows.h) choca en nombres con raylib.h (Rectangle, CloseWindow...)
    // si se incluyen juntos en el mismo .cpp, y este atajo no vale la pena esa
    // complicación. "start" lanza el proceso sin bloquear ni heredar esta consola.
    constexpr const char* kEditorPath = "../../../level-editor-csharp/bin/Debug/net9.0-windows/LevelEditor.exe";

    if (!FileExists(kEditorPath)) {
        TraceLog(LOG_WARNING, "Application: editor de niveles no encontrado en '%s' (¿esta compilado en Debug?)", kEditorPath);
        return;
    }

    // Idioma actual como argumento posicional (args[0] en Program.cs) --
    // así el editor abre ya en el mismo idioma que el juego en vez de
    // arrancar siempre en el último que se usó dentro del propio editor.
    std::string command = std::string("start \"\" \"") + kEditorPath + "\" " + m_localization.GetCurrentLanguage();
    std::system(command.c_str());
}

void Application::UpdateMenu() {
    // Atajo de teclado, redundante con el botón "Editor de Niveles" del
    // menú principal (ver MenuAction::OpenLevelEditor más abajo) -- se deja
    // como acceso rápido para quien ya sabe que existe.
    if (IsKeyPressed(KEY_F12)) {
        LaunchLevelEditor();
    }

    MenuAction action = MenuAction::None;
    switch (m_appState) {
        case AppState::MainMenu:     action = m_menuScreen.UpdateMainMenu();    break;
        case AppState::Options:      action = m_menuScreen.UpdateOptions(m_saveManager.Data().bgmVolume, m_saveManager.Data().sfxVolume); break;
        case AppState::Controls:     action = m_menuScreen.UpdateControls();    break;
        case AppState::Stats:        action = m_menuScreen.UpdateStats();       break;
        case AppState::Guide:        action = m_menuScreen.UpdateGuide();       break;
        case AppState::LevelSelect:  action = m_menuScreen.UpdateLevelSelect(m_saveManager.Data().maxLevelUnlocked); break;
        default: break; // StoryMode/EndlessMode/Paused no llegan aquí (ver Run())
    }

    // Mientras Opciones está abierta (venga del menú principal o de la
    // pausa), cada frame vuelca el volumen a MusicController/AudioSettings y
    // lo reaplica a lo que ya esté cargado -- y al soltar el ratón sobre el
    // slider, persiste. Guardar en cada frame de arrastre sería una
    // escritura a disco 60 veces por segundo; al soltar es un solo punto,
    // igual de inmediato de cara al jugador.
    if (m_appState == AppState::Options) {
        ApplyLiveAudioSettings();
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            m_saveManager.Save();
        }
    }

    switch (action) {
        case MenuAction::OpenLevelSelect: m_appState = AppState::LevelSelect; break;
        case MenuAction::StartStory:      StartStoryMode(m_menuScreen.GetSelectedLevel()); break;
        case MenuAction::StartEndless:    StartEndlessMode(); break;
        case MenuAction::OpenOptions:
            m_optionsReturnTo = AppState::MainMenu;
            m_appState = AppState::Options;
            break;
        case MenuAction::OpenControls:    m_appState = AppState::Controls; break;
        case MenuAction::OpenStats:       m_appState = AppState::Stats; break;
        case MenuAction::OpenGuide:
            m_guideReturnTo = AppState::MainMenu;
            m_appState = AppState::Guide;
            break;
        case MenuAction::OpenLevelEditor: LaunchLevelEditor(); break;
        case MenuAction::BackToMainMenu:
            // Opciones y Guía se pueden abrir desde el menú principal Y desde
            // la pausa, así que su "Volver" respeta de dónde vinieron; el
            // resto de pantallas (Stats, LevelSelect) siempre vuelven al menú
            // principal. m_appState todavía no se ha reasignado en este punto,
            // así que sigue siendo la pantalla que acaba de devolver la acción.
            if (m_appState == AppState::Options)    m_appState = m_optionsReturnTo;
            else if (m_appState == AppState::Guide) m_appState = m_guideReturnTo;
            else                                     m_appState = AppState::MainMenu;
            break;
        case MenuAction::BackToOptions:   m_appState = AppState::Options; break;
        case MenuAction::CycleLanguage:
            m_localization.CycleLanguage();
            // Se guarda al instante, no solo al morir/completar nivel: es la
            // única preferencia de UI persistente, y perderla si se cierra
            // el juego sin pasar por una partida sería un mal trago.
            m_saveManager.Data().currentLanguage = m_localization.GetCurrentLanguage();
            m_saveManager.Save();
            break;
        case MenuAction::Quit: m_quitRequested = true; break;
        default: break;
    }
}

void Application::DrawMenu() const {
    BeginDrawing();
    ClearBackground(Color{ 20, 20, 25, 255 });

    UiContext ui = BuildUiContext();
    switch (m_appState) {
        case AppState::MainMenu:    m_menuScreen.DrawMainMenu(ui); break;
        case AppState::Options:     m_menuScreen.DrawOptions(ui, m_saveManager.Data().bgmVolume, m_saveManager.Data().sfxVolume); break;
        case AppState::Controls:    m_menuScreen.DrawControls(ui); break;
        case AppState::Stats:       m_menuScreen.DrawStats(ui, m_saveManager.Data()); break;
        case AppState::Guide:       m_menuScreen.DrawGuide(ui); break;
        case AppState::LevelSelect: m_menuScreen.DrawLevelSelect(ui, m_saveManager.Data().maxLevelUnlocked); break;
        default: break;
    }

    EndDrawing();
}

// --- Gameplay (StoryMode / EndlessMode) ---

void Application::ClearTransientEffects() {
    m_camera.ClearShake();
    m_hitStopTimer.Start(0.0f);
}

void Application::UpdateGameplay(float dt) {
    m_music->Update();

    m_camera.Tick(dt);
    m_hitStopTimer.Tick(dt);
    m_particles.Update(dt);

    if (m_level.player != nullptr) {
        m_camera.FollowTarget(m_level.player->GetPosition());
    }

    if (m_appState == AppState::Paused) {
        UpdatePauseMenu();
        return;
    }

    if (m_matchState == GameState::WaitingToStart) {
        UpdateWaitingToStart();
        return;
    }

    if (m_matchState != GameState::Gameplay) {
        HandleGameplayPauseInput();
        return;
    }

    // Solo se puede pausar con la partida realmente en curso -- durante
    // GameOver/Victory el rebote de arriba ya se ha llevado el frame, así
    // que ESC ahí no hace nada (se usa 'R' para esas pantallas). Botón
    // Start del mando como alternativa a ESC (ver ctrl_pause en la
    // pantalla de Controles).
    bool pausePressed = IsKeyPressed(KEY_ESCAPE)
        || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT));
    if (pausePressed) {
        m_pausedFromState = m_appState;
        m_appState = AppState::Paused;
        // Un hit-stop o un temblor a medias no debe quedarse esperando al
        // otro lado de la pausa: si el jugador se va a Opciones o a la Guía,
        // UpdateGameplay deja de correr y esos timers ni siquiera se
        // descuentan, así que al volver reaparecerían congelados.
        ClearTransientEffects();
        return;
    }

    UpdateActiveMatch(dt);
}

void Application::UpdateWaitingToStart() {
    // El Player NO se actualiza durante la espera -- ni movimiento, ni dash,
    // ni ataque -- para que "Pulsa ATACAR para empezar" sea literal: no se
    // puede merodear ni golpear al aire antes de que la partida arranque de
    // verdad. Solo se escucha el propio botón de Atacar, fuera de la FSM
    // del Player (que ni siquiera llega a correr este frame).
    bool attackPressed = IsKeyPressed(KEY_SPACE)
        || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN));
    if (attackPressed) {
        m_matchState = GameState::Gameplay;
    }
}

void Application::UpdatePauseMenu() {
    MenuAction action = m_menuScreen.UpdatePause();
    switch (action) {
        case MenuAction::ResumeGame:
            m_appState = m_pausedFromState;
            break;
        case MenuAction::OpenOptions:
            m_optionsReturnTo = AppState::Paused;
            m_appState = AppState::Options;
            break;
        case MenuAction::OpenGuide:
            // Vuelve a la PAUSA, no al juego ni al menú principal: consultar
            // el glosario a media partida no puede costar el nivel en curso.
            m_guideReturnTo = AppState::Paused;
            m_appState = AppState::Guide;
            break;
        case MenuAction::BackToMainMenu:
            m_music->Stop();
            m_appState = AppState::MainMenu;
            break;
        default:
            break;
    }
}

void Application::ApplyLiveAudioSettings() {
    m_music->SetVolume(m_saveManager.Data().bgmVolume);
    AudioSettings::SetSfxVolume(m_saveManager.Data().sfxVolume);
    m_menuScreen.RefreshSfxVolume();

    // El resto solo importa si hay una partida cargada de verdad (se abrió
    // Opciones desde la pausa): un Player/Enemy/ExplosiveBarrel ya
    // construido tiene su Sound cargado con el volumen de cuando se creó, y
    // no vuelve a leerlo por sí solo.
    if (m_level.player) m_level.player->RefreshSfxVolume();
    for (auto& enemy : m_level.enemies) enemy->RefreshSfxVolume();
    for (auto& barrel : m_level.barrels) barrel->RefreshSfxVolume();
}

void Application::HandleGameplayPauseInput() {
    // Botón Select del mando como alternativa a 'R' (ver ctrl_retry en la
    // pantalla de Controles).
    bool retryPressed = IsKeyPressed(KEY_R)
        || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_LEFT));
    if (!retryPressed) return;

    // Infinito: 'R' rearranca el modo en el sitio (contador a cero, dificultad
    // reiniciada) en vez de devolver al menú. Volver al menú para tener que
    // navegar de nuevo hasta "Modo Infinito" era pura fricción entre intento
    // e intento; el récord ya se ha guardado al morir, así que no se pierde
    // nada. Para salir sigue estando ESC -> Salir al Menú Principal.
    if (m_matchState == GameState::GameOver && m_appState == AppState::EndlessMode) {
        StartEndlessMode();
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

void Application::CollectPickups() {
    // Los tres hacen lo mismo salvo qué efecto aplican: recorrer la lista,
    // probar el AABB del Player y sacar el elemento. Se comparte el barrido
    // con una lambda genérica en vez de repetir tres veces el mismo
    // erase-en-bucle con iteradores (donde es fácil olvidar el ++it).
    BoundingBox playerBox = m_level.player->GetBoundingBox();

    auto collect = [&playerBox](auto& container, auto&& onCollected) {
        for (auto it = container.begin(); it != container.end(); ) {
            if (CollisionMath::AABBIntersects(playerBox, (*it)->GetBoundingBox())) {
                onCollected(**it);
                it = container.erase(it);
            } else {
                ++it;
            }
        }
    };

    collect(m_level.gears, [this](const Gear&) {
        if (m_appState == AppState::EndlessMode) m_endlessDirector.OnGearCollected();
    });

    collect(m_level.powerUps, [this](const PowerUp& powerUp) {
        m_level.player->ApplyPowerUp(powerUp.GetType());
    });

    collect(m_level.healthKits, [this](const HealthKit& kit) {
        m_level.player->Heal(kit.GetHealAmount());
        m_saveManager.Data().healthKitsUsed++;
    });
}

void Application::ResolveBarrelExplosions() {
    // Se repite hasta que ningún barril quede por resolver, en vez de una
    // sola pasada: la explosión de un barril puede detonar a otro que está
    // ANTES en el vector y que este bucle ya ha dejado atrás. Con una sola
    // pasada, ese barril quedaba marcado como explotado, el erase-remove del
    // final del frame se lo llevaba, y su área no se aplicaba nunca -- una
    // explosión visible que no hacía daño a nada.
    //
    // ConsumeExplosion() es de un solo uso por barril, así que el bucle
    // termina siempre: cada vuelta resuelve al menos uno y ninguno se puede
    // resolver dos veces. La guarda de seguridad es el propio número de
    // barriles, que solo decrece.
    bool resolvedAny = true;
    while (resolvedAny) {
        resolvedAny = false;

        // Índice, no iterador/referencia: ApplyAreaDamage no toca el tamaño
        // del vector, pero recorrerlo por índice deja explícito que aquí no
        // se puede invalidar nada aunque en el futuro sí lo tocara.
        for (size_t i = 0; i < m_level.barrels.size(); i++) {
            if (!m_level.barrels[i]->ConsumeExplosion()) continue;

            Vector3 center = m_level.barrels[i]->GetPosition();
            CombatSystem::ApplyAreaDamage(center, ExplosiveBarrel::kExplosionRadius,
                                           ExplosiveBarrel::kExplosionDamage,
                                           *m_level.player, m_level.enemies, m_level.barrels);
            m_particles.Emit(center, GetRandomValue(15, 25));
            m_saveManager.Data().barrelsExploded++;
            resolvedAny = true;
        }
    }
}

void Application::UpdateActiveMatch(float dt) {
    if (!m_level.player) return;

    // Hit-stop: Player/Enemy ven dt = 0 (FSM y animación congeladas) mientras
    // el timer sigue corriendo con el dt real de Application::UpdateGameplay.
    float entityDt = m_hitStopTimer.IsActive() ? 0.0f : dt;

    m_level.player->Update(entityDt);

    for (auto& enemy : m_level.enemies) {
        enemy->NotifyPlayerPosition(m_level.player->GetPosition());
    }

    constexpr float kHitStopDuration = 0.05f;
    constexpr float kHitShakeDuration = 0.1f;
    constexpr float kHitShakeIntensity = 0.15f;
    constexpr int kMinHitParticles = 5;
    constexpr int kMaxHitParticles = 10;

    std::vector<MeleeHitResult> meleeHits = CombatSystem::ResolveMeleeAttack(*m_level.player, m_level.enemies, m_level.obstacles);
    if (!meleeHits.empty()) {
        // Una vez por swing, no por enemigo golpeado -- TriggerHitStop ya es
        // idempotente (toma el máximo) y AddShake reescribe los mismos
        // valores cada vez, así que llamarlos por enemigo no cambiaría nada
        // salvo repetir trabajo.
        TriggerHitStop(kHitStopDuration);
        m_camera.AddShake(kHitShakeDuration, kHitShakeIntensity);

        for (const MeleeHitResult& hit : meleeHits) {
            // La mitad de chispas si la placa del Shielder paró el golpe: se
            // ve que ha conectado algo, pero no como un impacto limpio.
            int particleCount = GetRandomValue(kMinHitParticles, kMaxHitParticles);
            m_particles.Emit(hit.impactPoint, hit.blocked ? particleCount / 2 : particleCount);

            if (hit.hitEnemy && !hit.hitEnemy->IsAlive()) {
                m_saveManager.Data().zombiesKilled++;
                DropTable::RollEnemyDrop(m_level, hit.hitEnemy->GetPosition(),
                                          m_appState == AppState::EndlessMode);
            }
        }
    } else if (auto barrelHit = CombatSystem::ResolveMeleeAttackOnBarrels(*m_level.player, m_level.barrels)) {
        // Mismo juice que golpear a un enemigo; la propia explosión (si el
        // barril llega a 0 HP) se resuelve más abajo, tras el update.
        TriggerHitStop(kHitStopDuration);
        m_camera.AddShake(kHitShakeDuration, kHitShakeIntensity);
        m_particles.Emit(*barrelHit, GetRandomValue(kMinHitParticles, kMaxHitParticles));
    }

    CombatSystem::ApplyBufferAuras(m_level.enemies);

    for (auto& enemy : m_level.enemies) {
        enemy->Update(entityDt);

        Projectile projectile;
        if (enemy->ConsumePendingProjectile(projectile)) {
            m_projectiles.push_back(projectile);
        }

        Vector3 puddlePosition;
        if (enemy->ConsumePendingPuddle(puddlePosition)) {
            m_puddles.push_back(MudPuddle{ puddlePosition, MudPuddle::kLifetime });
        }

        if (enemy->ConsumeExplosionTrigger()) {
            CombatSystem::ApplyAreaDamage(enemy->GetPosition(), Enemy::kExplodeRadius, enemy->GetExplosionDamage(),
                                           *m_level.player, m_level.enemies, m_level.barrels);
            m_particles.Emit(enemy->GetPosition(), GetRandomValue(15, 25));
        }
    }

    CombatSystem::ResolveEnemyAttacks(*m_level.player, m_level.enemies);
    CombatSystem::UpdateProjectiles(entityDt, m_projectiles, *m_level.player, m_level.obstacles, m_level.barrels);

    for (auto& hazard : m_level.hazards) hazard->Update(entityDt);
    CombatSystem::ApplyHazardDamage(m_level.hazards, *m_level.player);
    CombatSystem::UpdateMudPuddles(entityDt, m_puddles, *m_level.player);
    CombatSystem::UpdateElectricTiles(entityDt, m_level.electricTiles, *m_level.player, m_level.enemies);

    for (auto& spawner : m_spawners) {
        spawner.Update(entityDt, m_level.enemies);
    }

    if (m_appState == AppState::EndlessMode) {
        m_endlessDirector.Update(entityDt, m_spawners);
    }

    ResolveBarrelExplosions();

    m_level.barrels.erase(
        std::remove_if(m_level.barrels.begin(), m_level.barrels.end(),
                        [](const std::unique_ptr<ExplosiveBarrel>& b) { return b->HasExploded(); }),
        m_level.barrels.end());

    // Los Spawner guardan Enemy* NO propietarios de los que han generado.
    // Se les hace soltar los que van a desaparecer AQUÍ, pegado al
    // erase-remove, en vez de confiar en que su propio Update (que corre
    // antes en este mismo frame) ya los haya purgado por IsAlive(): así el
    // invariante "ningún puntero sobrevive a su Enemy" vive junto al borrado
    // y no se rompe si algún día se reordena este bucle.
    for (Spawner& spawner : m_spawners) spawner.ForgetDestroyedEnemies();

    // Corpse cleanup: saca del vector a los enemigos que ya terminaron su
    // fade-out (ver Enemy::UpdateDead).
    m_level.enemies.erase(
        std::remove_if(m_level.enemies.begin(), m_level.enemies.end(),
                        [](const std::unique_ptr<Enemy>& e) { return e->IsPendingDestruction(); }),
        m_level.enemies.end());

    // Recolección solo si el jugador sigue vivo: si murió en este mismo frame
    // (combate, hazard o baldosa), un cadáver no debe seguir recogiendo. Sin
    // esta guarda, morir encima de un engranaje sumaba puntuación a una
    // partida ya perdida y se tragaba el objeto que había debajo.
    if (m_level.player->IsAlive()) {
        CollectPickups();
    }

    // --- Condiciones de fin de partida ---
    // La muerte se comprueba ANTES que la victoria y sale con return: morir
    // en el mismo frame en que se pisa la puerta con el último engranaje ya
    // recogido es Game Over, no victoria. Es la resolución deliberada del
    // empate, no un accidente del orden de las dos comprobaciones.
    if (!m_level.player->IsAlive()) {
        TraceLog(LOG_INFO, "Application: GAME OVER");
        m_matchState = GameState::GameOver;
        m_music->Stop();

        if (m_appState == AppState::EndlessMode && m_endlessDirector.GetScore() > m_saveManager.Data().highScore) {
            m_saveManager.Data().highScore = m_endlessDirector.GetScore();
        }
        m_saveManager.Save();
        return;
    }

    // Infinito no tiene puerta de victoria: solo termina al morir el
    // jugador (ver HandleGameplayPauseInput).
    if (m_appState == AppState::StoryMode && m_level.door && m_level.gears.empty() &&
        CollisionMath::AABBIntersects(m_level.player->GetBoundingBox(), m_level.door->GetBoundingBox())) {
        TraceLog(LOG_INFO, "Application: VICTORY");
        m_matchState = GameState::Victory;
        m_music->Stop();
        m_saveManager.Save();
    }
}

void Application::DrawGroundGrid() const {
    // Cuadrícula holográfica estilo Tron en vez del DrawGrid gris por
    // defecto de raylib -- raylib no define CYAN, así que se usa SKYBLUE
    // (mismo sustituto ya usado para el contorno de Obstacle).
    //
    // 32 (arena de -16 a +16), no 20: los muros perimetrales de todos los
    // niveles están ahora en ±16 (ver assets/data/*.json), así que una
    // cuadrícula de ±10 dejaba media arena sin suelo visible. Si se vuelve a
    // mover el perímetro, este número tiene que acompañarlo.
    constexpr int slices = 32;
    constexpr float spacing = 1.0f;
    constexpr float halfSize = (slices * spacing) / 2.0f;
    Color gridColor = Fade(SKYBLUE, 0.3f);

    for (int i = -slices / 2; i <= slices / 2; i++) {
        float offset = i * spacing;
        DrawLine3D(Vector3{ offset, 0.0f, -halfSize }, Vector3{ offset, 0.0f, halfSize }, gridColor);
        DrawLine3D(Vector3{ -halfSize, 0.0f, offset }, Vector3{ halfSize, 0.0f, offset }, gridColor);
    }
}

void Application::DrawGameplay() const {
    BeginDrawing();
    ClearBackground(Color{ 30, 30, 35, 255 });

    BeginMode3D(m_camera.Get());
    DrawGroundGrid();

    // Los charcos van antes que el Player/los enemigos: son una capa de
    // suelo, cualquier cosa que camine encima tiene que taparlos.
    for (const MudPuddle& puddle : m_puddles) {
        // Se apaga con la vida que le queda, así se ve que va a secarse
        // antes de que desaparezca de golpe bajo los pies del jugador.
        float fade = puddle.lifetime / MudPuddle::kLifetime;
        Vector3 base{ puddle.position.x, 0.015f, puddle.position.z };
        DrawCylinder(base, MudPuddle::kRadius, MudPuddle::kRadius, 0.02f, 20, Fade(Color{ 60, 170, 50, 255 }, 0.55f * fade));
        DrawCylinderWires(base, MudPuddle::kRadius, MudPuddle::kRadius, 0.02f, 20, Fade(LIME, 0.8f * fade));
    }

    if (m_level.player) m_level.player->Draw();
    for (auto& enemy : m_level.enemies) enemy->Draw();
    for (auto& obstacle : m_level.obstacles) obstacle->Draw();
    for (auto& hazard : m_level.hazards) hazard->Draw();
    for (auto& tile : m_level.electricTiles) tile->Draw();
    for (const Spawner& spawner : m_spawners) spawner.Draw();
    for (auto& gear : m_level.gears) gear->Draw();
    for (auto& powerUp : m_level.powerUps) powerUp->Draw();
    for (auto& healthKit : m_level.healthKits) healthKit->Draw();
    for (auto& barrel : m_level.barrels) barrel->Draw();
    for (const Projectile& projectile : m_projectiles) DrawSphere(projectile.position, Projectile::kRadius, YELLOW);
    if (m_level.door) m_level.door->Draw();
    m_particles.Draw();
    EndMode3D();

    UiContext ui = BuildUiContext();
    HudContext hudContext;
    // Durante la pausa, m_appState es Paused y no dice de qué modo venimos --
    // el HUD se sigue dibujando detrás del overlay, así que sin esto pausar
    // en Infinito le cambiaba la cabecera a "Nivel N" y el marcador dual al
    // formato de Historia.
    hudContext.appState = (m_appState == AppState::Paused) ? m_pausedFromState : m_appState;
    hudContext.totalGears = m_totalGears;
    hudContext.currentStoryLevel = m_currentLevel;
    hudContext.endlessScore = m_endlessDirector.GetScore();
    hudContext.endlessHighScore = m_saveManager.Data().highScore;
    m_hud.DrawHud(ui, m_level, hudContext, m_camera.Get());

    switch (m_matchState) {
        case GameState::WaitingToStart:
            m_hud.DrawCenteredOverlay(ui, "ready_title", SKYBLUE, "ready_subtitle");
            break;
        case GameState::GameOver:
            // Un solo subtítulo para los dos modos: desde que Infinito
            // reintenta in situ (ver HandleGameplayPauseInput), 'R' significa
            // lo mismo en ambos y el "vuelve al menú" de antes mentiría.
            m_hud.DrawCenteredOverlay(ui, "gameover_title", RED, "gameover_retry");
            break;
        case GameState::Victory:
            m_hud.DrawCenteredOverlay(ui, "victory_title", GREEN, "victory_continue");
            break;
        default:
            break;
    }

    if (m_appState == AppState::Paused) {
        m_menuScreen.DrawPause(ui);
    }

    // DrawFPS() de raylib dibuja a tamaño fijo (20px) con su propia fuente
    // interna -- no se puede agrandar. Se sustituye por un DrawTextEx
    // propio, en la esquina superior DERECHA (la izquierda ya la ocupa el
    // texto de Vida de DrawHud, ver HudRenderer.cpp) para que el contador
    // de FPS crezca junto al resto de texto del HUD sin solaparse.
    constexpr float kFpsTextSize = LocalizationManager::kFontSizeFps;
    const Font& fpsFont = ui.localization.GetFontForSize(kFpsTextSize);
    const char* fpsText = TextFormat("FPS: %d", GetFPS());
    Vector2 fpsDim = MeasureTextEx(fpsFont, fpsText, kFpsTextSize, 1.0f);
    DrawTextEx(fpsFont, fpsText, Vector2{ GetScreenWidth() - fpsDim.x - 10.0f, 10.0f }, kFpsTextSize, 1.0f, LIME);
    EndDrawing();
}

void Application::TriggerHitStop(float duration) {
    // max, no asignación directa: un segundo golpe durante un hit-stop ya
    // activo no debe acortarlo.
    m_hitStopTimer.Start(std::max(m_hitStopTimer.Remaining(), duration));
}

void Application::Run() {
    while (!WindowShouldClose() && !m_quitRequested) {
        switch (m_appState) {
            case AppState::MainMenu:
            case AppState::Options:
            case AppState::Controls:
            case AppState::Stats:
            case AppState::Guide:
            case AppState::LevelSelect:
                UpdateMenu();
                DrawMenu();
                break;

            case AppState::StoryMode:
            case AppState::EndlessMode:
            case AppState::Paused:
                UpdateGameplay(GetFrameTime());
                DrawGameplay();
                break;
        }
    }
}
