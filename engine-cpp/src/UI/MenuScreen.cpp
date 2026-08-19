#include "MenuScreen.h"

namespace {

constexpr float kButtonWidth = 300.0f;
constexpr float kButtonHeight = 60.0f;
constexpr float kButtonGap = 20.0f;

} // namespace

Rectangle MenuScreen::StackedButton(int index, int count) {
    float screenW = static_cast<float>(GetScreenWidth());
    float screenH = static_cast<float>(GetScreenHeight());
    float totalHeight = count * kButtonHeight + (count - 1) * kButtonGap;
    float startY = (screenH - totalHeight) / 2.0f;
    float x = (screenW - kButtonWidth) / 2.0f;
    float y = startY + index * (kButtonHeight + kButtonGap);
    return Rectangle{ x, y, kButtonWidth, kButtonHeight };
}

Rectangle MenuScreen::VolumeSliderBounds() {
    float screenW = static_cast<float>(GetScreenWidth());
    float screenH = static_cast<float>(GetScreenHeight());
    return Rectangle{ (screenW - kButtonWidth) / 2.0f, screenH / 2.0f - 180.0f, kButtonWidth, 30.0f };
}

Rectangle MenuScreen::BackButtonBounds() {
    float screenW = static_cast<float>(GetScreenWidth());
    float screenH = static_cast<float>(GetScreenHeight());
    return Rectangle{ (screenW - kButtonWidth) / 2.0f, screenH - 100.0f, kButtonWidth, kButtonHeight };
}

const char* MenuScreen::NativeLanguageName(const std::string& code) {
    // Nombres nativos, no traducidos -- el nombre propio de un idioma se
    // muestra igual sea cual sea el idioma activo de la UI (como en
    // cualquier selector de idioma real).
    if (code == "en") return "English";
    if (code == "jp") return "日本語";
    return "Español";
}

bool MenuScreen::IsButtonClicked(Rectangle bounds) {
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), bounds);
}

std::vector<MenuScreen::MenuButton> MenuScreen::BuildMainMenuButtons() const {
    return {
        { StackedButton(0, 5), "menu_story", MenuAction::OpenLevelSelect, false },
        { StackedButton(1, 5), "menu_endless", MenuAction::StartEndless, false },
        { StackedButton(2, 5), "menu_options", MenuAction::OpenOptions, false },
        { StackedButton(3, 5), "menu_stats", MenuAction::OpenStats, false },
        { StackedButton(4, 5), "menu_quit", MenuAction::Quit, false },
    };
}

std::vector<MenuScreen::MenuButton> MenuScreen::BuildLevelSelectButtons(int maxUnlockedLevel) const {
    int unlockedCount = maxUnlockedLevel;
    if (unlockedCount > kStoryLevelCount) unlockedCount = kStoryLevelCount;
    if (unlockedCount < 1) unlockedCount = 1; // nunca menos de 1: el nivel 1 siempre está desbloqueado

    int buttonCount = unlockedCount + 1; // + botón Volver
    std::vector<MenuButton> buttons;
    buttons.reserve(static_cast<size_t>(buttonCount));

    for (int level = 1; level <= unlockedCount; level++) {
        MenuButton button{ StackedButton(level - 1, buttonCount), "level_label", MenuAction::StartStory, false };
        button.levelNumber = level;
        buttons.push_back(button);
    }
    buttons.push_back({ StackedButton(unlockedCount, buttonCount), "levelselect_back", MenuAction::BackToMainMenu, false });

    return buttons;
}

std::vector<MenuScreen::MenuButton> MenuScreen::BuildOptionsButtons() const {
    Rectangle slider = VolumeSliderBounds();
    float x = slider.x;
    float startY = slider.y + slider.height + 40.0f;

    return {
        { Rectangle{ x, startY, kButtonWidth, kButtonHeight }, "options_language", MenuAction::CycleLanguage, true },
        { Rectangle{ x, startY + (kButtonHeight + kButtonGap), kButtonWidth, kButtonHeight }, "options_controls", MenuAction::OpenControls, false },
        { Rectangle{ x, startY + 2.0f * (kButtonHeight + kButtonGap), kButtonWidth, kButtonHeight }, "options_back", MenuAction::BackToMainMenu, false },
    };
}

// --- Navegación y dibujado compartidos ---

MenuAction MenuScreen::UpdateButtonList(const std::vector<MenuButton>& buttons) {
    if (buttons.empty()) return MenuAction::None;

    int count = static_cast<int>(buttons.size());
    if (m_selectedIndex < 0 || m_selectedIndex >= count) {
        m_selectedIndex = 0;
    }

    // Ratón: pasar por encima de un botón lo selecciona también, para que
    // ratón y mando/teclado no se desincronicen visualmente.
    Vector2 mouse = GetMousePosition();
    for (int i = 0; i < count; i++) {
        if (CheckCollisionPointRec(mouse, buttons[static_cast<size_t>(i)].bounds)) {
            m_selectedIndex = i;
            break;
        }
    }

    if (m_navCooldown > 0.0f) m_navCooldown -= GetFrameTime();

    bool gamepadReady = IsGamepadAvailable(0);
    bool axisDown = gamepadReady && GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y) > 0.6f;
    bool axisUp = gamepadReady && GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y) < -0.6f;

    bool moveDown = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)
        || (gamepadReady && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN))
        || (axisDown && m_navCooldown <= 0.0f);
    bool moveUp = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)
        || (gamepadReady && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP))
        || (axisUp && m_navCooldown <= 0.0f);

    constexpr float kNavRepeatDelay = 0.2f; // solo se usa para el eje analógico, ver arriba
    if (moveDown) { m_selectedIndex = (m_selectedIndex + 1) % count; m_navCooldown = kNavRepeatDelay; }
    if (moveUp)   { m_selectedIndex = (m_selectedIndex - 1 + count) % count; m_navCooldown = kNavRepeatDelay; }

    bool confirmPressed = IsKeyPressed(KEY_ENTER)
        || (gamepadReady && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN));
    if (confirmPressed) {
        const MenuButton& chosen = buttons[static_cast<size_t>(m_selectedIndex)];
        if (chosen.levelNumber > 0) m_lastSelectedLevel = chosen.levelNumber;
        return chosen.action;
    }

    for (const MenuButton& button : buttons) {
        if (IsButtonClicked(button.bounds)) {
            if (button.levelNumber > 0) m_lastSelectedLevel = button.levelNumber;
            return button.action;
        }
    }

    return MenuAction::None;
}

void MenuScreen::DrawButtonList(const std::vector<MenuButton>& buttons, const UiContext& ui) const {
    for (int i = 0; i < static_cast<int>(buttons.size()); i++) {
        const MenuButton& button = buttons[static_cast<size_t>(i)];
        std::string label = ui.localization.GetText(button.labelKey);
        if (button.isLanguageButton) {
            label += ": ";
            label += NativeLanguageName(ui.localization.GetCurrentLanguage());
        }
        if (button.levelNumber > 0) {
            label += " ";
            label += std::to_string(button.levelNumber);
        }
        DrawButton(button.bounds, label.c_str(), i == m_selectedIndex, ui);
    }
}

void MenuScreen::DrawButton(Rectangle bounds, const char* label, bool selected, const UiContext& ui) const {
    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    DrawRectangleRec(bounds, (selected || hovered) ? Color{ 80, 80, 95, 255 } : Color{ 50, 50, 60, 255 });
    DrawRectangleLinesEx(bounds, selected ? 3.0f : 2.0f, selected ? SKYBLUE : RAYWHITE);

    constexpr float textSize = 24.0f;
    Vector2 textDim = MeasureTextEx(ui.font, label, textSize, 1.0f);
    DrawTextEx(ui.font, label,
               Vector2{ bounds.x + (bounds.width - textDim.x) / 2.0f, bounds.y + (bounds.height - textSize) / 2.0f },
               textSize, 1.0f, RAYWHITE);
}

// --- Menú principal ---

MenuAction MenuScreen::UpdateMainMenu() {
    return UpdateButtonList(BuildMainMenuButtons());
}

void MenuScreen::DrawMainMenu(const UiContext& ui) const {
    const char* title = ui.localization.GetText("menu_title");
    constexpr float titleSize = 50.0f;
    Vector2 titleDim = MeasureTextEx(ui.font, title, titleSize, 1.0f);
    DrawTextEx(ui.font, title, Vector2{ (GetScreenWidth() - titleDim.x) / 2.0f, 100.0f }, titleSize, 1.0f, RAYWHITE);

    DrawButtonList(BuildMainMenuButtons(), ui);
}

// --- Opciones ---

MenuAction MenuScreen::UpdateOptions() {
    Rectangle slider = VolumeSliderBounds();
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), slider)) {
        float ratio = (GetMousePosition().x - slider.x) / slider.width;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        SetMasterVolume(ratio);
    }

    return UpdateButtonList(BuildOptionsButtons());
}

void MenuScreen::DrawOptions(const UiContext& ui) const {
    const char* title = ui.localization.GetText("options_title");
    constexpr float titleSize = 50.0f;
    Vector2 titleDim = MeasureTextEx(ui.font, title, titleSize, 1.0f);
    DrawTextEx(ui.font, title, Vector2{ (GetScreenWidth() - titleDim.x) / 2.0f, 80.0f }, titleSize, 1.0f, RAYWHITE);

    float volume = GetMasterVolume();
    Rectangle slider = VolumeSliderBounds();

    std::string volumeLabel = std::string(ui.localization.GetText("options_volume")) +
                               TextFormat(": %d%%", static_cast<int>(volume * 100.0f));
    Vector2 volDim = MeasureTextEx(ui.font, volumeLabel.c_str(), 20.0f, 1.0f);
    DrawTextEx(ui.font, volumeLabel.c_str(), Vector2{ (GetScreenWidth() - volDim.x) / 2.0f, slider.y - 30.0f }, 20.0f, 1.0f, RAYWHITE);

    DrawRectangleRec(slider, Color{ 40, 40, 48, 255 });
    DrawRectangleRec(Rectangle{ slider.x, slider.y, slider.width * volume, slider.height }, SKYBLUE);
    DrawRectangleLinesEx(slider, 2.0f, RAYWHITE);

    DrawButtonList(BuildOptionsButtons(), ui);
}

// --- Controles ---

MenuAction MenuScreen::UpdateControls() {
    std::vector<MenuButton> buttons = { { BackButtonBounds(), "controls_back", MenuAction::BackToOptions, false } };
    return UpdateButtonList(buttons);
}

void MenuScreen::DrawControls(const UiContext& ui) const {
    const char* title = ui.localization.GetText("controls_title");
    constexpr float titleSize = 50.0f;
    Vector2 titleDim = MeasureTextEx(ui.font, title, titleSize, 1.0f);
    DrawTextEx(ui.font, title, Vector2{ (GetScreenWidth() - titleDim.x) / 2.0f, 80.0f }, titleSize, 1.0f, RAYWHITE);

    float screenCenter = static_cast<float>(GetScreenWidth()) / 2.0f;
    float colActionX = screenCenter - 350.0f;
    float colKbX = screenCenter - 50.0f;
    float colPadX = screenCenter + 220.0f;
    float rowY = 200.0f;
    constexpr float rowHeight = 60.0f;
    constexpr float textSize = 22.0f;

    DrawTextEx(ui.font, ui.localization.GetText("controls_column_kb"), Vector2{ colKbX, rowY }, textSize, 1.0f, SKYBLUE);
    DrawTextEx(ui.font, ui.localization.GetText("controls_column_pad"), Vector2{ colPadX, rowY }, textSize, 1.0f, SKYBLUE);
    rowY += rowHeight;

    const char* actionKeys[] = { "ctrl_move", "ctrl_attack", "ctrl_dash" };
    const char* kbKeys[] = { "ctrl_move_kb", "ctrl_attack_kb", "ctrl_dash_kb" };
    const char* padKeys[] = { "ctrl_move_pad", "ctrl_attack_pad", "ctrl_dash_pad" };

    for (int i = 0; i < 3; i++) {
        DrawTextEx(ui.font, ui.localization.GetText(actionKeys[i]), Vector2{ colActionX, rowY }, textSize, 1.0f, RAYWHITE);
        DrawTextEx(ui.font, ui.localization.GetText(kbKeys[i]), Vector2{ colKbX, rowY }, textSize, 1.0f, RAYWHITE);
        DrawTextEx(ui.font, ui.localization.GetText(padKeys[i]), Vector2{ colPadX, rowY }, textSize, 1.0f, RAYWHITE);
        rowY += rowHeight;
    }

    std::vector<MenuButton> buttons = { { BackButtonBounds(), "controls_back", MenuAction::BackToOptions, false } };
    DrawButtonList(buttons, ui);
}

// --- Estadísticas ---

MenuAction MenuScreen::UpdateStats() {
    std::vector<MenuButton> buttons = { { BackButtonBounds(), "stats_back", MenuAction::BackToMainMenu, false } };
    return UpdateButtonList(buttons);
}

void MenuScreen::DrawStats(const UiContext& ui, const SaveData& saveData) const {
    const char* title = ui.localization.GetText("stats_title");
    constexpr float titleSize = 50.0f;
    Vector2 titleDim = MeasureTextEx(ui.font, title, titleSize, 1.0f);
    DrawTextEx(ui.font, title, Vector2{ (GetScreenWidth() - titleDim.x) / 2.0f, 80.0f }, titleSize, 1.0f, RAYWHITE);

    float screenCenter = static_cast<float>(GetScreenWidth()) / 2.0f;
    float labelX = screenCenter - 300.0f;
    float valueX = screenCenter + 120.0f;
    float rowY = 220.0f;
    constexpr float rowHeight = 50.0f;
    constexpr float textSize = 24.0f;

    auto drawRow = [&](const char* labelKey, int value) {
        DrawTextEx(ui.font, ui.localization.GetText(labelKey), Vector2{ labelX, rowY }, textSize, 1.0f, RAYWHITE);
        DrawTextEx(ui.font, TextFormat("%d", value), Vector2{ valueX, rowY }, textSize, 1.0f, GOLD);
        rowY += rowHeight;
    };

    drawRow("stats_highscore", saveData.highScore);
    drawRow("stats_zombies", saveData.zombiesKilled);
    drawRow("stats_barrels", saveData.barrelsExploded);
    drawRow("stats_healthkits", saveData.healthKitsUsed);
    drawRow("stats_maxlevel", saveData.maxLevelUnlocked);

    std::vector<MenuButton> buttons = { { BackButtonBounds(), "stats_back", MenuAction::BackToMainMenu, false } };
    DrawButtonList(buttons, ui);
}

// --- Selector de nivel (Modo Historia) ---

MenuAction MenuScreen::UpdateLevelSelect(int maxUnlockedLevel) {
    return UpdateButtonList(BuildLevelSelectButtons(maxUnlockedLevel));
}

void MenuScreen::DrawLevelSelect(const UiContext& ui, int maxUnlockedLevel) const {
    const char* title = ui.localization.GetText("levelselect_title");
    constexpr float titleSize = 50.0f;
    Vector2 titleDim = MeasureTextEx(ui.font, title, titleSize, 1.0f);
    DrawTextEx(ui.font, title, Vector2{ (GetScreenWidth() - titleDim.x) / 2.0f, 80.0f }, titleSize, 1.0f, RAYWHITE);

    DrawButtonList(BuildLevelSelectButtons(maxUnlockedLevel), ui);
}
