#include "HudRenderer.h"
#include <string>

void HudRenderer::DrawModeHeader(const UiContext& ui, const HudContext& hud) const {
    constexpr float kHeaderSize = LocalizationManager::kFontSizeHud;
    const Font& font = ui.localization.GetFontForSize(kHeaderSize);

    std::string header = (hud.appState == AppState::EndlessMode)
        ? ui.localization.GetText("hud_endless_title")
        : TextFormat("%s %d", ui.localization.GetText("hud_level"), hud.currentStoryLevel);

    Vector2 dim = MeasureTextEx(font, header.c_str(), kHeaderSize, 1.0f);
    Color color = (hud.appState == AppState::EndlessMode) ? SKYBLUE : RAYWHITE;
    DrawTextEx(font, header.c_str(), Vector2{ (GetScreenWidth() - dim.x) / 2.0f, 10.0f }, kHeaderSize, 1.0f, color);
}

void HudRenderer::DrawHud(const UiContext& ui, const LevelData& level, const HudContext& hud, const Camera3D& camera) const {
    if (!level.player) return;

    DrawModeHeader(ui, hud);

    // --- Barra de HP del jugador ---
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

    // --- Engranajes: puntuación y récord en Infinito, recolectados/total en Historia ---
    const char* gearsLabel = ui.localization.GetText("hud_gears");
    std::string gearsText = (hud.appState == AppState::EndlessMode)
        ? TextFormat("%s: %d   %s: %d", gearsLabel, hud.endlessScore,
                     ui.localization.GetText("hud_endless_best"), hud.endlessHighScore)
        : TextFormat("%s: %d / %d", gearsLabel, hud.totalGears - static_cast<int>(level.gears.size()), hud.totalGears);
    DrawTextEx(hudFont, gearsText.c_str(), Vector2{ static_cast<float>(barX), static_cast<float>(barY + barHeight + 10) }, kHudTextSize, 1.0f, ORANGE);

    // --- Power-ups activos, una línea por efecto ---
    float effectY = static_cast<float>(barY + barHeight + 10) + kHudTextSize + 6.0f;
    auto drawEffectRow = [&](const char* labelKey, PowerUpType type, float remaining, bool showSeconds) {
        std::string text = showSeconds
            ? TextFormat("%s: %.1fs", ui.localization.GetText(labelKey), remaining)
            : ui.localization.GetText(labelKey);
        DrawTextEx(hudFont, text.c_str(), Vector2{ static_cast<float>(barX), effectY }, kHudTextSize, 1.0f, PowerUp::TypeColor(type));
        effectY += kHudTextSize + 6.0f;
    };

    if (level.player->GetOverclockRemaining() > 0.0f) {
        drawEffectRow("powerup_overclock", PowerUpType::Overclock, level.player->GetOverclockRemaining(), true);
    }
    if (level.player->GetFrenzyRemaining() > 0.0f) {
        drawEffectRow("powerup_frenzy", PowerUpType::Frenzy, level.player->GetFrenzyRemaining(), true);
    }
    if (level.player->HasShield()) {
        drawEffectRow("powerup_shield", PowerUpType::Shield, 0.0f, false);
    }

    // --- Barra de HP flotante sobre cada enemigo dañado ---
    for (auto& enemy : level.enemies) {
        if (!enemy->IsAlive()) continue;
        if (enemy->GetHP() >= enemy->GetMaxHP()) continue; // solo tras el primer golpe

        Vector3 worldPos = enemy->GetPosition();
        worldPos.y += 1.0f;
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
