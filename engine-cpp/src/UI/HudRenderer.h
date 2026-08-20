#pragma once
#include "raylib.h"
#include "UiContext.h"
#include "../Core/AppState.h"
#include "../IO/LevelLoader.h"

// Todo lo que el HUD necesita saber de la partida y que NO vive en LevelData.
// Agrupado en una struct en vez de seguir alargando la lista de parámetros de
// DrawHud (ya iban seis, y este cambio añadía tres más): así añadir un dato
// al HUD no obliga a tocar la firma ni la llamada.
struct HudContext {
    AppState appState = AppState::StoryMode;
    int totalGears = 0;
    int currentStoryLevel = 1;   // solo se muestra en StoryMode
    int endlessScore = 0;        // engranajes de esta partida de Infinito
    int endlessHighScore = 0;    // récord persistido (SaveData::highScore)
};

// Dibuja el HUD de partida (barra de HP, contador de engranajes, efectos
// activos, cabecera de nivel/modo, barras flotantes de HP de los enemigos) y
// el overlay centrado de fin de partida (GAME OVER / VICTORIA) -- extraído de
// Application para que la orquestación de estados no cargue también con la
// disposición en pantalla de cada elemento de UI, igual que MenuScreen ya
// hace con los menús.
class HudRenderer {
public:
    void DrawHud(const UiContext& ui, const LevelData& level, const HudContext& hud, const Camera3D& camera) const;

    void DrawCenteredOverlay(const UiContext& ui, const char* titleKey, Color titleColor, const char* subtitleKey) const;

private:
    // Cabecera centrada arriba: "Nivel N" en Historia, el nombre del modo en
    // Infinito. Centrada a propósito -- la esquina superior izquierda ya la
    // ocupa la vida y la derecha el contador de FPS.
    void DrawModeHeader(const UiContext& ui, const HudContext& hud) const;
};
