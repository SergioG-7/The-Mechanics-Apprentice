#pragma once
#include "raylib.h"
#include "UiContext.h"
#include "../Core/AppState.h"
#include "../IO/LevelLoader.h"

// Datos de la partida que necesita el HUD y no vienen en LevelData.
struct HudContext {
    AppState appState = AppState::StoryMode;
    int totalGears = 0;
    int currentStoryLevel = 1;   // solo se muestra en StoryMode
    int endlessScore = 0;
    int endlessHighScore = 0;
};

// Dibuja el HUD de partida (vida, engranajes, efectos activos) y el overlay de fin de partida.
class HudRenderer {
public:
    void DrawHud(const UiContext& ui, const LevelData& level, const HudContext& hud, const Camera3D& camera) const;

    void DrawCenteredOverlay(const UiContext& ui, const char* titleKey, Color titleColor, const char* subtitleKey) const;

private:
    // Cabecera centrada arriba: "Nivel N" en Historia, o el nombre del modo en Infinito.
    void DrawModeHeader(const UiContext& ui, const HudContext& hud) const;
};
