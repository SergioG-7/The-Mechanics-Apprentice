#pragma once
#include "raylib.h"
#include "UiContext.h"
#include "../Core/AppState.h"
#include "../Core/EndlessDirector.h"
#include "../IO/LevelLoader.h"

// Dibuja el HUD de partida (barra de HP, contador de engranajes, barras
// flotantes de HP de los enemigos) y el overlay centrado de fin de partida
// (GAME OVER / VICTORIA) -- extraído de Application para que la orquestación
// de estados no cargue también con la disposición en pantalla de cada
// elemento de UI, igual que MenuScreen ya hace con los menús.
class HudRenderer {
public:
    void DrawHud(const UiContext& ui, const LevelData& level, int totalGears,
                 AppState appState, const EndlessDirector& endlessDirector, const Camera3D& camera) const;

    void DrawCenteredOverlay(const UiContext& ui, const char* titleKey, Color titleColor, const char* subtitleKey) const;
};
