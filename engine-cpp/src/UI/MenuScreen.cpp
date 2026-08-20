#include "MenuScreen.h"
#include "../Core/AudioSettings.h"
#include <algorithm>

namespace {

constexpr float kButtonWidth = 300.0f;
constexpr float kButtonHeight = 60.0f;
constexpr float kButtonGap = 20.0f;

constexpr float kEditorButtonWidth = 260.0f; // suficiente para "Editor de Niveles" (el más largo de los 3 idiomas) a 24px
constexpr float kEditorButtonHeight = 40.0f;
constexpr float kEditorButtonMargin = 20.0f;

} // namespace

MenuScreen::~MenuScreen() {
    if (m_clickSound.frameCount > 0) UnloadSound(m_clickSound);
}

void MenuScreen::LoadSfx() {
    m_clickSound = LoadSound("assets/audio/sfx/click.mp3");
    RefreshSfxVolume();
}

void MenuScreen::RefreshSfxVolume() {
    if (m_clickSound.frameCount > 0) SetSoundVolume(m_clickSound, AudioSettings::GetSfxVolume());
}

void MenuScreen::PlayClickSound() const {
    if (m_clickSound.frameCount > 0) PlaySound(m_clickSound);
}

Rectangle MenuScreen::StackedButton(int index, int count) {
    float screenW = static_cast<float>(GetScreenWidth());
    float screenH = static_cast<float>(GetScreenHeight());
    float totalHeight = count * kButtonHeight + (count - 1) * kButtonGap;
    float startY = (screenH - totalHeight) / 2.0f;
    float x = (screenW - kButtonWidth) / 2.0f;
    float y = startY + index * (kButtonHeight + kButtonGap);
    return Rectangle{ x, y, kButtonWidth, kButtonHeight };
}

Rectangle MenuScreen::BgmVolumeSliderBounds() {
    // Fija, no relativa a screenH/2: el título "OPCIONES" también se dibuja
    // a una Y fija (80.0f, ver DrawOptions) con tamaño 50 -- anclar aquí al
    // título en vez de al centro de la pantalla es lo que garantiza que no
    // se pisen sea cual sea la resolución. 200.0f deja ~60px de aire bajo el
    // título (que termina sobre los 140) antes de la etiqueta de esta barra
    // (que se dibuja en bounds.y - 30, ver DrawVolumeSlider).
    float screenW = static_cast<float>(GetScreenWidth());
    return Rectangle{ (screenW - kButtonWidth) / 2.0f, 200.0f, kButtonWidth, 30.0f };
}

Rectangle MenuScreen::SfxVolumeSliderBounds() {
    Rectangle bgm = BgmVolumeSliderBounds();
    return Rectangle{ bgm.x, bgm.y + 80.0f, bgm.width, bgm.height };
}

Rectangle MenuScreen::BackButtonBounds() {
    float screenW = static_cast<float>(GetScreenWidth());
    float screenH = static_cast<float>(GetScreenHeight());
    return Rectangle{ (screenW - kButtonWidth) / 2.0f, screenH - 100.0f, kButtonWidth, kButtonHeight };
}

namespace {
// 150.0f, no centrado a pantalla completa como StackedButton: con
// kLevelSelectVirtualRows filas virtuales fijas (5 niveles + 1 fila de
// paginación), el centrado puro dejaría el primer botón pegado al título
// (y=80). Ancla en su lugar justo debajo del título, mismo margen que ya
// usa MainMenu entre su título y su primer botón.
constexpr float kLevelSelectStartY = 150.0f;
constexpr float kLevelNavGap = 20.0f;
} // namespace

Rectangle MenuScreen::EditorButtonBounds() {
    return Rectangle{ kEditorButtonMargin, kEditorButtonMargin, kEditorButtonWidth, kEditorButtonHeight };
}

Rectangle MenuScreen::LevelSelectNavButtonBounds(bool isNext) {
    // Fila compartida (la última de las virtuales), partida en dos mitades
    // -- Anterior a la izquierda, Siguiente a la derecha, con un hueco entre
    // ambas. Si solo una de las dos existe esta página, se queda en su lado
    // natural en vez de recentrarse: más simple, y no hay confusión posible
    // sobre cuál es cuál.
    float screenW = static_cast<float>(GetScreenWidth());
    float x = (screenW - kButtonWidth) / 2.0f;
    float y = kLevelSelectStartY + (kLevelSelectVirtualRows - 1) * (kButtonHeight + kButtonGap);
    float halfWidth = (kButtonWidth - kLevelNavGap) / 2.0f;
    float buttonX = isNext ? x + halfWidth + kLevelNavGap : x;
    return Rectangle{ buttonX, y, halfWidth, kButtonHeight };
}

bool MenuScreen::IsButtonClicked(Rectangle bounds) {
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), bounds);
}

std::vector<MenuScreen::MenuButton> MenuScreen::BuildMainMenuButtons() const {
    // El botón del editor de niveles NO está aquí -- vive fuera del flujo
    // vertical apilado, anclado a la esquina superior izquierda (ver
    // EditorButtonBounds/DrawMainMenu/UpdateMainMenu), para no empujar estos
    // 5 botones hasta pisar el título.
    return {
        { StackedButton(0, 5), "menu_story", MenuAction::OpenLevelSelect, false },
        { StackedButton(1, 5), "menu_endless", MenuAction::StartEndless, false },
        { StackedButton(2, 5), "menu_options", MenuAction::OpenOptions, false },
        { StackedButton(3, 5), "menu_stats", MenuAction::OpenStats, false },
        { StackedButton(4, 5), "menu_quit", MenuAction::Quit, false },
    };
}

std::vector<MenuScreen::MenuButton> MenuScreen::BuildLevelSelectButtons(int maxUnlockedLevel) const {
    int unlockedCount = std::max(1, maxUnlockedLevel); // nunca menos de 1: el nivel 1 siempre está desbloqueado
    int totalPages = (unlockedCount + kLevelsPerPage - 1) / kLevelsPerPage; // división entera hacia arriba
    int page = std::clamp(m_levelSelectPage, 0, totalPages - 1);

    int pageStart = page * kLevelsPerPage + 1;
    int pageEnd = std::min(pageStart + kLevelsPerPage - 1, unlockedCount);
    int levelsThisPage = pageEnd - pageStart + 1;

    bool hasPrev = page > 0;
    bool hasNext = page < totalPages - 1;

    // Vertical, apilados uno debajo del otro como el resto del menú. La fila
    // (0..4) de cada botón de nivel usa el número VIRTUAL de filas fijo
    // (kLevelSelectVirtualRows), no levelsThisPage -- así la posición no
    // cambia entre páginas aunque una tenga menos niveles que otra. Página
    // Anterior/Siguiente solo si de verdad hay a dónde ir -- no se muestran
    // deshabilitados, directamente no existen. Volver en su posición fija
    // de siempre, igual que en Controles/Estadísticas.
    std::vector<MenuButton> buttons;
    buttons.reserve(static_cast<size_t>(levelsThisPage) + 3);

    for (int level = pageStart; level <= pageEnd; level++) {
        int row = level - pageStart;
        Rectangle bounds{ (static_cast<float>(GetScreenWidth()) - kButtonWidth) / 2.0f,
                           kLevelSelectStartY + row * (kButtonHeight + kButtonGap), kButtonWidth, kButtonHeight };
        MenuButton button{ bounds, "level_label", MenuAction::StartStory, false };
        button.levelNumber = level;
        buttons.push_back(button);
    }
    if (hasPrev) buttons.push_back({ LevelSelectNavButtonBounds(false), "levelselect_prev", MenuAction::LevelSelectPrevPage, false });
    if (hasNext) buttons.push_back({ LevelSelectNavButtonBounds(true), "levelselect_next", MenuAction::LevelSelectNextPage, false });
    buttons.push_back({ BackButtonBounds(), "levelselect_back", MenuAction::BackToMainMenu, false });

    return buttons;
}

std::vector<MenuScreen::MenuButton> MenuScreen::BuildOptionsButtons() const {
    Rectangle slider = SfxVolumeSliderBounds();
    float x = slider.x;
    float startY = slider.y + slider.height + 40.0f;

    return {
        { Rectangle{ x, startY, kButtonWidth, kButtonHeight }, "options_language", MenuAction::CycleLanguage, true },
        { Rectangle{ x, startY + (kButtonHeight + kButtonGap), kButtonWidth, kButtonHeight }, "options_controls", MenuAction::OpenControls, false },
        { Rectangle{ x, startY + 2.0f * (kButtonHeight + kButtonGap), kButtonWidth, kButtonHeight }, "options_back", MenuAction::BackToMainMenu, false },
    };
}

std::vector<MenuScreen::MenuButton> MenuScreen::BuildPauseButtons() const {
    return {
        { StackedButton(0, 3), "pause_resume", MenuAction::ResumeGame, false },
        { StackedButton(1, 3), "menu_options", MenuAction::OpenOptions, false },
        { StackedButton(2, 3), "pause_exit", MenuAction::BackToMainMenu, false },
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
        PlayClickSound();
        return chosen.action;
    }

    for (const MenuButton& button : buttons) {
        if (IsButtonClicked(button.bounds)) {
            if (button.levelNumber > 0) m_lastSelectedLevel = button.levelNumber;
            PlayClickSound();
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
            // "language_name" es el nombre nativo que cada idioma declara de
            // sí mismo en su propio JSON (ver assets/lang/*.json) -- se lee
            // siempre del idioma ACTIVO, así que nunca hace falta indexar
            // por código: un botón "Idioma: Español" en español, "Language:
            // English" en inglés, "言語: 日本語" en japonés.
            label += ui.localization.GetText("language_name");
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

    constexpr float textSize = LocalizationManager::kFontSizeBody;
    const Font& font = ui.localization.GetFontForSize(textSize);
    Vector2 textDim = MeasureTextEx(font, label, textSize, 1.0f);
    DrawTextEx(font, label,
               Vector2{ bounds.x + (bounds.width - textDim.x) / 2.0f, bounds.y + (bounds.height - textSize) / 2.0f },
               textSize, 1.0f, RAYWHITE);
}

// --- Menú principal ---

MenuAction MenuScreen::UpdateMainMenu() {
    // Fuera de UpdateButtonList a propósito: el botón del editor no es parte
    // de la navegación por teclado/mando de la lista apilada (ver
    // EditorButtonBounds), así que su clic se comprueba aparte, antes de la
    // lista normal.
    if (IsButtonClicked(EditorButtonBounds())) {
        PlayClickSound();
        return MenuAction::OpenLevelEditor;
    }
    return UpdateButtonList(BuildMainMenuButtons());
}

void MenuScreen::DrawMainMenu(const UiContext& ui) const {
    const char* title = ui.localization.GetText("menu_title");
    constexpr float titleSize = LocalizationManager::kFontSizeTitle;
    Vector2 titleDim = MeasureTextEx(ui.localization.GetFontForSize(titleSize), title, titleSize, 1.0f);
    DrawTextEx(ui.localization.GetFontForSize(titleSize), title, Vector2{ (GetScreenWidth() - titleDim.x) / 2.0f, 100.0f }, titleSize, 1.0f, RAYWHITE);

    DrawButtonList(BuildMainMenuButtons(), ui);

    // Independiente de la lista apilada (ver EditorButtonBounds): nunca
    // aparece "seleccionado" (false fijo), solo resaltado al pasar el ratón
    // por encima (DrawButton ya comprueba el hover por su cuenta).
    DrawButton(EditorButtonBounds(), ui.localization.GetText("menu_editor"), false, ui);
}

// --- Opciones ---

void MenuScreen::UpdateVolumeSliderDrag(Rectangle bounds, float& volume) {
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) || !CheckCollisionPointRec(GetMousePosition(), bounds)) return;

    float ratio = (GetMousePosition().x - bounds.x) / bounds.width;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    volume = ratio;
}

MenuAction MenuScreen::UpdateOptions(float& bgmVolume, float& sfxVolume) {
    UpdateVolumeSliderDrag(BgmVolumeSliderBounds(), bgmVolume);
    UpdateVolumeSliderDrag(SfxVolumeSliderBounds(), sfxVolume);

    return UpdateButtonList(BuildOptionsButtons());
}

void MenuScreen::DrawVolumeSlider(const UiContext& ui, Rectangle bounds, const char* labelKey, float volume) const {
    std::string label = std::string(ui.localization.GetText(labelKey)) +
                         TextFormat(": %d%%", static_cast<int>(volume * 100.0f));
    constexpr float labelSize = LocalizationManager::kFontSizeSliderLabel;
    const Font& font = ui.localization.GetFontForSize(labelSize);
    Vector2 labelDim = MeasureTextEx(font, label.c_str(), labelSize, 1.0f);
    DrawTextEx(font, label.c_str(), Vector2{ (GetScreenWidth() - labelDim.x) / 2.0f, bounds.y - 30.0f }, labelSize, 1.0f, RAYWHITE);

    DrawRectangleRec(bounds, Color{ 40, 40, 48, 255 });
    DrawRectangleRec(Rectangle{ bounds.x, bounds.y, bounds.width * volume, bounds.height }, SKYBLUE);
    DrawRectangleLinesEx(bounds, 2.0f, RAYWHITE);
}

void MenuScreen::DrawOptions(const UiContext& ui, float bgmVolume, float sfxVolume) const {
    const char* title = ui.localization.GetText("options_title");
    constexpr float titleSize = LocalizationManager::kFontSizeTitle;
    Vector2 titleDim = MeasureTextEx(ui.localization.GetFontForSize(titleSize), title, titleSize, 1.0f);
    DrawTextEx(ui.localization.GetFontForSize(titleSize), title, Vector2{ (GetScreenWidth() - titleDim.x) / 2.0f, 80.0f }, titleSize, 1.0f, RAYWHITE);

    DrawVolumeSlider(ui, BgmVolumeSliderBounds(), "options_volume_bgm", bgmVolume);
    DrawVolumeSlider(ui, SfxVolumeSliderBounds(), "options_volume_sfx", sfxVolume);

    DrawButtonList(BuildOptionsButtons(), ui);
}

// --- Pausa ---

MenuAction MenuScreen::UpdatePause() {
    return UpdateButtonList(BuildPauseButtons());
}

void MenuScreen::DrawPause(const UiContext& ui) const {
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    // Mismo overlay que DrawCenteredOverlay (fin de partida): oscurece la
    // escena congelada detrás sin ocultarla del todo.
    DrawRectangle(0, 0, screenW, screenH, Color{ 0, 0, 0, 150 });

    const char* title = ui.localization.GetText("pause_title");
    constexpr float titleSize = LocalizationManager::kFontSizeTitle;
    Vector2 titleDim = MeasureTextEx(ui.localization.GetFontForSize(titleSize), title, titleSize, 1.0f);
    DrawTextEx(ui.localization.GetFontForSize(titleSize), title, Vector2{ (screenW - titleDim.x) / 2.0f, 150.0f }, titleSize, 1.0f, RAYWHITE);

    DrawButtonList(BuildPauseButtons(), ui);
}

// --- Controles ---

MenuAction MenuScreen::UpdateControls() {
    std::vector<MenuButton> buttons = { { BackButtonBounds(), "controls_back", MenuAction::BackToOptions, false } };
    return UpdateButtonList(buttons);
}

void MenuScreen::DrawControls(const UiContext& ui) const {
    const char* title = ui.localization.GetText("controls_title");
    constexpr float titleSize = LocalizationManager::kFontSizeTitle;
    Vector2 titleDim = MeasureTextEx(ui.localization.GetFontForSize(titleSize), title, titleSize, 1.0f);
    DrawTextEx(ui.localization.GetFontForSize(titleSize), title, Vector2{ (GetScreenWidth() - titleDim.x) / 2.0f, 80.0f }, titleSize, 1.0f, RAYWHITE);

    float screenCenter = static_cast<float>(GetScreenWidth()) / 2.0f;
    float colActionX = screenCenter - 350.0f;
    float colKbX = screenCenter - 50.0f;
    float colPadX = screenCenter + 220.0f;
    float rowY = 200.0f;
    constexpr float rowHeight = 60.0f;
    constexpr float textSize = LocalizationManager::kFontSizeControlsRow;
    const Font& font = ui.localization.GetFontForSize(textSize);

    DrawTextEx(font, ui.localization.GetText("controls_column_kb"), Vector2{ colKbX, rowY }, textSize, 1.0f, SKYBLUE);
    DrawTextEx(font, ui.localization.GetText("controls_column_pad"), Vector2{ colPadX, rowY }, textSize, 1.0f, SKYBLUE);
    rowY += rowHeight;

    const char* actionKeys[] = { "ctrl_move", "ctrl_attack", "ctrl_dash", "ctrl_pause", "ctrl_retry" };
    const char* kbKeys[] = { "ctrl_move_kb", "ctrl_attack_kb", "ctrl_dash_kb", "ctrl_pause_kb", "ctrl_retry_kb" };
    const char* padKeys[] = { "ctrl_move_pad", "ctrl_attack_pad", "ctrl_dash_pad", "ctrl_pause_pad", "ctrl_retry_pad" };

    for (int i = 0; i < 5; i++) {
        DrawTextEx(font, ui.localization.GetText(actionKeys[i]), Vector2{ colActionX, rowY }, textSize, 1.0f, RAYWHITE);
        DrawTextEx(font, ui.localization.GetText(kbKeys[i]), Vector2{ colKbX, rowY }, textSize, 1.0f, RAYWHITE);
        DrawTextEx(font, ui.localization.GetText(padKeys[i]), Vector2{ colPadX, rowY }, textSize, 1.0f, RAYWHITE);
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
    constexpr float titleSize = LocalizationManager::kFontSizeTitle;
    Vector2 titleDim = MeasureTextEx(ui.localization.GetFontForSize(titleSize), title, titleSize, 1.0f);
    DrawTextEx(ui.localization.GetFontForSize(titleSize), title, Vector2{ (GetScreenWidth() - titleDim.x) / 2.0f, 80.0f }, titleSize, 1.0f, RAYWHITE);

    float screenCenter = static_cast<float>(GetScreenWidth()) / 2.0f;
    float labelX = screenCenter - 300.0f;
    float valueX = screenCenter + 120.0f;
    float rowY = 220.0f;
    constexpr float rowHeight = 50.0f;
    constexpr float textSize = LocalizationManager::kFontSizeBody;
    const Font& font = ui.localization.GetFontForSize(textSize);

    auto drawRow = [&](const char* labelKey, int value) {
        DrawTextEx(font, ui.localization.GetText(labelKey), Vector2{ labelX, rowY }, textSize, 1.0f, RAYWHITE);
        DrawTextEx(font, TextFormat("%d", value), Vector2{ valueX, rowY }, textSize, 1.0f, GOLD);
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
    // Clamp de página ANTES de construir/leer botones: si maxUnlockedLevel
    // pudiera bajar entre visitas (no ocurre hoy, pero es barato cubrirlo),
    // una página ya no válida no debe quedarse fuera de rango.
    int unlockedCount = std::max(1, maxUnlockedLevel);
    int totalPages = (unlockedCount + kLevelsPerPage - 1) / kLevelsPerPage;
    m_levelSelectPage = std::clamp(m_levelSelectPage, 0, totalPages - 1);

    // Página Anterior/Siguiente son estado puramente de MenuScreen -- se
    // consumen aquí mismo y nunca llegan a Application como acción real.
    MenuAction action = UpdateButtonList(BuildLevelSelectButtons(maxUnlockedLevel));
    if (action == MenuAction::LevelSelectNextPage) {
        m_levelSelectPage++;
        return MenuAction::None;
    }
    if (action == MenuAction::LevelSelectPrevPage) {
        m_levelSelectPage--;
        return MenuAction::None;
    }
    return action;
}

void MenuScreen::DrawLevelSelect(const UiContext& ui, int maxUnlockedLevel) const {
    const char* title = ui.localization.GetText("levelselect_title");
    constexpr float titleSize = LocalizationManager::kFontSizeTitle;
    Vector2 titleDim = MeasureTextEx(ui.localization.GetFontForSize(titleSize), title, titleSize, 1.0f);
    DrawTextEx(ui.localization.GetFontForSize(titleSize), title, Vector2{ (GetScreenWidth() - titleDim.x) / 2.0f, 80.0f }, titleSize, 1.0f, RAYWHITE);

    DrawButtonList(BuildLevelSelectButtons(maxUnlockedLevel), ui);
}
