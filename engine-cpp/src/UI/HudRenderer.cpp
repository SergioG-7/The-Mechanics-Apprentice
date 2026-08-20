#include "HudRenderer.h"
#include <string>

void HudRenderer::DrawHud(const UiContext& ui, const LevelData& level, int totalGears,
                           AppState appState, const EndlessDirector& endlessDirector, const Camera3D& camera) const {
    if (!level.player) return;

    // --- Barra de HP del Player, con etiqueta encima -- antes la barra no
    // llevaba número, solo color, y en el playtest pidieron que "Vida" fuera
    // uno de los textos grandes del HUD. ---
    constexpr int barX = 10, barY = 60, barWidth = 200, barHeight = 20;
    constexpr float kHudTextSize = LocalizationManager::kFontSizeHud;
    const Font& hudFont = ui.localization.GetFontForSize(kHudTextSize);
    float hpRatio = level.player->GetHP() / level.player->GetMaxHP();

    std::string hpText = TextFormat("%s: %d / %d", ui.localization.GetText("hud_hp"),
                                     static_cast<int>(level.player->GetHP()), static_cast<int>(level.player->GetMaxHP()));
    DrawTextEx(hudFont, hpText.c_str(), Vector2{ static_cast<float>(barX), 10.0f }, kHudTextSize, 1.0f, RAYWHITE);

    DrawRectangle(barX, barY, barWidth, barHeight, DARKGRAY);
    DrawRectangle(barX, barY, static_cast<int>(barWidth * hpRatio), barHeight, RED);
    DrawRectangleLines(barX, barY, barWidth, barHeight, BLACK);

    // --- Engranajes: en Infinito es la puntuación (sin total fijo); en
    // Historia, recolectados / total del nivel. ---
    const char* gearsLabel = ui.localization.GetText("hud_gears");
    std::string gearsText = (appState == AppState::EndlessMode)
        ? TextFormat("%s: %d", gearsLabel, endlessDirector.GetScore())
        : TextFormat("%s: %d / %d", gearsLabel, totalGears - static_cast<int>(level.gears.size()), totalGears);
    DrawTextEx(hudFont, gearsText.c_str(), Vector2{ static_cast<float>(barX), static_cast<float>(barY + barHeight + 10) }, kHudTextSize, 1.0f, ORANGE);

    // --- Barra de HP flotante sobre cada Enemy dañado ---
    for (auto& enemy : level.enemies) {
        if (!enemy->IsAlive()) continue;               // un zombie derrotado no necesita barra
        if (enemy->GetHP() >= enemy->GetMaxHP()) continue; // solo tras el primer golpe, para no saturar

        Vector3 worldPos = enemy->GetPosition();
        worldPos.y += 1.0f; // encima del cubo, no sobre su centro
        Vector2 screenPos = GetWorldToScreen(worldPos, camera);

        constexpr int enemyBarWidth = 40, enemyBarHeight = 6;
        int ex = static_cast<int>(screenPos.x) - enemyBarWidth / 2;
        int ey = static_cast<int>(screenPos.y);
        float enemyHpRatio = enemy->GetHP() / enemy->GetMaxHP();

        DrawRectangle(ex, ey, enemyBarWidth, enemyBarHeight, DARKGRAY);
        DrawRectangle(ex, ey, static_cast<int>(enemyBarWidth * enemyHpRatio), enemyBarHeight, RED);
    }
}

void HudRenderer::DrawCenteredOverlay(const UiContext& ui, const char* titleKey, Color titleColor, const char* subtitleKey) const {
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    DrawRectangle(0, 0, screenW, screenH, Color{ 0, 0, 0, 150 });

    const char* title = ui.localization.GetText(titleKey);
    constexpr float titleSize = LocalizationManager::kFontSizeOverlayTitle;
    const Font& titleFont = ui.localization.GetFontForSize(titleSize);
    Vector2 titleDim = MeasureTextEx(titleFont, title, titleSize, 1.0f);
    DrawTextEx(titleFont, title, Vector2{ (screenW - titleDim.x) / 2.0f, screenH / 2.0f - 60.0f }, titleSize, 1.0f, titleColor);

    const char* subtitle = ui.localization.GetText(subtitleKey);
    constexpr float subSize = LocalizationManager::kFontSizeOverlaySubtitle;
    const Font& subFont = ui.localization.GetFontForSize(subSize);
    Vector2 subDim = MeasureTextEx(subFont, subtitle, subSize, 1.0f);
    DrawTextEx(subFont, subtitle, Vector2{ (screenW - subDim.x) / 2.0f, screenH / 2.0f + 50.0f }, subSize, 1.0f, RAYWHITE);
}
