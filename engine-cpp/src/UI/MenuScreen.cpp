#include "MenuScreen.h"
#include "../Core/AudioSettings.h"
#include "../Entities/PowerUp.h" // PowerUp::TypeColor: los iconos del glosario usan el MISMO color que el pickup real
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

float MenuScreen::StackedListStartY(int count) {
    float screenH = static_cast<float>(GetScreenHeight());
    float totalHeight = count * kButtonHeight + (count - 1) * kButtonGap;
    float centered = (screenH - totalHeight) / 2.0f;

    // El tope gana sobre el centrado, nunca al revés: una lista larga baja
    // hasta despegarse del título aunque eso la descentre. Ver kMenuContentTop.
    return std::max(centered, kMenuContentTop);
}

Rectangle MenuScreen::StackedButton(int index, int count) {
    float screenW = static_cast<float>(GetScreenWidth());
    float x = (screenW - kButtonWidth) / 2.0f;
    float y = StackedListStartY(count) + index * (kButtonHeight + kButtonGap);
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
    // EditorButtonBounds/DrawMainMenu/UpdateMainMenu).
    //
    // 6 filas desde que existe "Guía". El centrado vertical puro ya no cabe
    // bajo el título con tantas: de eso se encarga StackedListStartY, que
    // ancla la lista en kMenuContentTop en cuanto el centrado se metería
    // dentro del título. Añadir una séptima seguirá funcionando (la lista
    // crece hacia abajo), pero habrá que comprobar que la última no se salga
    // por debajo: 6 filas terminan en y=630 de 720.
    constexpr int kRows = 6;
    return {
        { StackedButton(0, kRows), "menu_story", MenuAction::OpenLevelSelect, false },
        { StackedButton(1, kRows), "menu_endless", MenuAction::StartEndless, false },
        { StackedButton(2, kRows), "menu_guide", MenuAction::OpenGuide, false },
        { StackedButton(3, kRows), "menu_options", MenuAction::OpenOptions, false },
        { StackedButton(4, kRows), "menu_stats", MenuAction::OpenStats, false },
        { StackedButton(5, kRows), "menu_quit", MenuAction::Quit, false },
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
    // 4 filas desde que la Guía también se abre en pausa. StackedListStartY
    // sigue centrando (4 filas caben de sobra: 480px de 720), pero es lo que
    // garantiza que si mañana se añade una quinta no se meta bajo el título
    // "PAUSA", que se dibuja a y=150 y llega hasta los 200.
    constexpr int kRows = 4;
    return {
        { StackedButton(0, kRows), "pause_resume", MenuAction::ResumeGame, false },
        { StackedButton(1, kRows), "menu_guide", MenuAction::OpenGuide, false },
        { StackedButton(2, kRows), "menu_options", MenuAction::OpenOptions, false },
        { StackedButton(3, kRows), "pause_exit", MenuAction::BackToMainMenu, false },
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
    // 110, no 150: con el cuarto botón (Guía) la lista centrada arranca en
    // y=210, y el título a 150 terminaba justo en 200 -- 10px de aire, que a
    // ojo se lee como pegado. A 110 termina en 160 y quedan 50 limpios.
    DrawTextEx(ui.localization.GetFontForSize(titleSize), title, Vector2{ (screenW - titleDim.x) / 2.0f, 110.0f }, titleSize, 1.0f, RAYWHITE);

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

// --- Guía / Glosario ---

namespace {
// Layout de una fila del glosario. Todo se deriva del centro de la pantalla
// para que no dependa de una resolución concreta.
constexpr float kGuideFirstRowY = 190.0f;
constexpr float kGuideRowHeight = 52.0f;
constexpr float kGuideIconSize = 34.0f;
constexpr int kGuideMaxRows = 7; // la página más larga (Mecánicas y Enemigos tienen 7)
} // namespace

const std::vector<MenuScreen::GuideEntry>& MenuScreen::GuidePage(int page) {
    static const std::vector<GuideEntry> mechanics = {
        { GuideIcon::Door,         "guide_door_name",      "guide_door_desc" },
        { GuideIcon::Gear,         "guide_gear_name",      "guide_gear_desc" },
        { GuideIcon::Barrel,       "guide_barrel_name",    "guide_barrel_desc" },
        { GuideIcon::HealthKit,    "guide_healthkit_name", "guide_healthkit_desc" },
        { GuideIcon::Spikes,       "guide_spikes_name",    "guide_spikes_desc" },
        { GuideIcon::ElectricTile, "guide_electric_name",  "guide_electric_desc" },
        { GuideIcon::MudPuddle,    "guide_mud_name",       "guide_mud_desc" },
    };
    static const std::vector<GuideEntry> enemies = {
        { GuideIcon::EnemyMelee,    "guide_tank_name",     "guide_tank_desc" },
        { GuideIcon::EnemyRunner,   "guide_runner_name",   "guide_runner_desc" },
        { GuideIcon::EnemySpitter,  "guide_spitter_name",  "guide_spitter_desc" },
        { GuideIcon::EnemyKamikaze, "guide_kamikaze_name", "guide_kamikaze_desc" },
        { GuideIcon::EnemyShielder, "guide_shielder_name", "guide_shielder_desc" },
        { GuideIcon::EnemyBuffer,   "guide_buffer_name",   "guide_buffer_desc" },
        { GuideIcon::EnemyTrapper,  "guide_trapper_name",  "guide_trapper_desc" },
    };
    static const std::vector<GuideEntry> powerUps = {
        { GuideIcon::PowerOverclock, "powerup_overclock", "powerup_overclock_desc" },
        { GuideIcon::PowerFrenzy,    "powerup_frenzy",    "powerup_frenzy_desc" },
        { GuideIcon::PowerShield,    "powerup_shield",    "powerup_shield_desc" },
    };

    switch (page) {
        case 1:  return enemies;
        case 2:  return powerUps;
        default: return mechanics;
    }
}

const char* MenuScreen::GuidePageTitleKey(int page) {
    switch (page) {
        case 1:  return "guide_cat_enemies";
        case 2:  return "guide_cat_powerups";
        default: return "guide_cat_mechanics";
    }
}

void MenuScreen::DrawGuideIcon(GuideIcon icon, Rectangle bounds) {
    // Mismos colores que la entidad real en 3D, en 2D y sin cámara: lo que
    // importa es que el jugador asocie color+silueta, no un render fiel.
    float cx = bounds.x + bounds.width / 2.0f;
    float cy = bounds.y + bounds.height / 2.0f;
    float r = bounds.width / 2.0f;

    auto drawEnemyIcon = [&](Color body) {
        // Silueta común de zombie: cuerpo redondeado y "hombros" rectos, para
        // que las siete variantes se lean como el mismo bicho teñido distinto.
        DrawRectangleRounded(Rectangle{ bounds.x + 4.0f, bounds.y + 2.0f, bounds.width - 8.0f, bounds.height - 4.0f }, 0.35f, 6, body);
        DrawRectangleRoundedLines(Rectangle{ bounds.x + 4.0f, bounds.y + 2.0f, bounds.width - 8.0f, bounds.height - 4.0f }, 0.35f, 6, BLACK);
    };

    switch (icon) {
        case GuideIcon::Door:
            DrawRectangleRec(Rectangle{ bounds.x + 6.0f, bounds.y, bounds.width - 12.0f, bounds.height }, Color{ 0, 228, 48, 120 });
            DrawRectangleLinesEx(Rectangle{ bounds.x + 6.0f, bounds.y, bounds.width - 12.0f, bounds.height }, 2.0f, GREEN);
            break;
        case GuideIcon::Gear:
            DrawPoly(Vector2{ cx, cy }, 6, r, 0.0f, GOLD);
            DrawPolyLines(Vector2{ cx, cy }, 6, r, 0.0f, YELLOW);
            DrawCircle(static_cast<int>(cx), static_cast<int>(cy), r * 0.35f, Color{ 30, 30, 35, 255 });
            break;
        case GuideIcon::Barrel:
            DrawRectangleRounded(Rectangle{ bounds.x + 7.0f, bounds.y + 2.0f, bounds.width - 14.0f, bounds.height - 4.0f }, 0.4f, 6, Color{ 178, 34, 34, 255 });
            DrawLineEx(Vector2{ bounds.x + 7.0f, cy }, Vector2{ bounds.x + bounds.width - 7.0f, cy }, 2.0f, ORANGE);
            break;
        case GuideIcon::HealthKit:
            DrawRectangleRec(bounds, GREEN);
            DrawRectangleRec(Rectangle{ cx - r * 0.55f, cy - r * 0.18f, r * 1.1f, r * 0.36f }, RAYWHITE);
            DrawRectangleRec(Rectangle{ cx - r * 0.18f, cy - r * 0.55f, r * 0.36f, r * 1.1f }, RAYWHITE);
            break;
        case GuideIcon::Spikes:
            DrawRectangleRec(bounds, Color{ 200, 90, 20, 255 });
            // DrawPoly de 3 lados en vez de DrawTriangle: raylib culea las
            // caras traseras, así que un DrawTriangle con el winding al revés
            // simplemente no se dibuja. DrawPoly emite el suyo correcto solo.
            // rotation -90 lo hace apuntar hacia arriba (en pantalla, Y crece
            // hacia abajo, así que "arriba" es -90, no +90).
            for (int i = 0; i < 3; i++) {
                float sx = bounds.x + 8.0f + i * (bounds.width - 16.0f) / 2.0f;
                DrawPoly(Vector2{ sx, cy }, 3, r * 0.45f, -90.0f, LIGHTGRAY);
            }
            break;
        case GuideIcon::ElectricTile:
            DrawRectangleRec(bounds, Color{ 45, 55, 75, 255 });
            DrawRectangleLinesEx(bounds, 2.0f, YELLOW);
            // Rayo en zeta con líneas gruesas -- mismo motivo que arriba para
            // no usar triángulos a mano.
            DrawLineEx(Vector2{ cx + r * 0.35f, cy - r * 0.7f }, Vector2{ cx - r * 0.25f, cy }, 3.0f, SKYBLUE);
            DrawLineEx(Vector2{ cx - r * 0.25f, cy }, Vector2{ cx + r * 0.25f, cy }, 3.0f, SKYBLUE);
            DrawLineEx(Vector2{ cx + r * 0.25f, cy }, Vector2{ cx - r * 0.35f, cy + r * 0.7f }, 3.0f, SKYBLUE);
            break;
        case GuideIcon::MudPuddle:
            DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), r, r * 0.7f, Color{ 60, 170, 50, 200 });
            DrawEllipseLines(static_cast<int>(cx), static_cast<int>(cy), r, r * 0.7f, LIME);
            break;

        case GuideIcon::EnemyMelee:    drawEnemyIcon(Color{ 150, 150, 165, 255 }); break;
        case GuideIcon::EnemyRunner:   drawEnemyIcon(Color{ 200, 210, 190, 255 }); break;
        case GuideIcon::EnemySpitter:  drawEnemyIcon(Color{ 190, 120, 235, 255 }); break;
        case GuideIcon::EnemyKamikaze: drawEnemyIcon(Color{ 255, 110, 100, 255 }); break;
        case GuideIcon::EnemyShielder:
            drawEnemyIcon(Color{ 120, 165, 235, 255 });
            // La placa por delante, que es lo que hay que reconocer en partida.
            DrawRectangleRec(Rectangle{ bounds.x, cy - r * 0.75f, 6.0f, r * 1.5f }, SKYBLUE);
            break;
        case GuideIcon::EnemyBuffer:
            drawEnemyIcon(Color{ 255, 210, 80, 255 });
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), r + 3.0f, GOLD);
            break;
        case GuideIcon::EnemyTrapper:
            drawEnemyIcon(Color{ 130, 235, 110, 255 });
            DrawCircle(static_cast<int>(bounds.x + bounds.width - 5.0f), static_cast<int>(bounds.y + 6.0f), 6.0f, Color{ 70, 210, 70, 255 });
            break;

        case GuideIcon::PowerOverclock:
            // Cono/flecha hacia arriba, igual que el pickup en 3D.
            DrawPoly(Vector2{ cx, cy }, 3, r, -90.0f, PowerUp::TypeColor(PowerUpType::Overclock));
            break;
        case GuideIcon::PowerFrenzy:
            DrawRectangleRec(Rectangle{ cx - r, cy - r * 0.22f, r * 2.0f, r * 0.44f }, PowerUp::TypeColor(PowerUpType::Frenzy));
            DrawRectangleRec(Rectangle{ cx - r * 0.22f, cy - r, r * 0.44f, r * 2.0f }, PowerUp::TypeColor(PowerUpType::Frenzy));
            break;
        case GuideIcon::PowerShield:
            DrawRectangleRec(Rectangle{ cx - r * 0.55f, cy - r * 0.75f, r * 1.1f, r * 1.5f }, PowerUp::TypeColor(PowerUpType::Shield));
            DrawRectangleRec(Rectangle{ cx - r * 0.2f, cy - r, r * 0.4f, r * 0.3f }, RAYWHITE);
            break;
    }
}

std::vector<MenuScreen::MenuButton> MenuScreen::BuildGuideButtons() const {
    // Misma fila compartida de Anterior/Siguiente que el selector de nivel,
    // pero anclada bajo la última fila de contenido en vez de a una rejilla
    // de filas virtuales: aquí el número de filas SÍ cambia por página
    // (Power-ups solo tiene 3), y aun así los botones no deben moverse.
    float screenW = static_cast<float>(GetScreenWidth());
    float x = (screenW - kButtonWidth) / 2.0f;
    float navY = kGuideFirstRowY + kGuideMaxRows * kGuideRowHeight + 12.0f;
    float halfWidth = (kButtonWidth - kLevelNavGap) / 2.0f;
    constexpr float kNavHeight = 44.0f;

    std::vector<MenuButton> buttons;
    if (m_guidePage > 0) {
        buttons.push_back({ Rectangle{ x, navY, halfWidth, kNavHeight }, "levelselect_prev", MenuAction::GuidePrevPage, false });
    }
    if (m_guidePage < kGuidePageCount - 1) {
        buttons.push_back({ Rectangle{ x + halfWidth + kLevelNavGap, navY, halfWidth, kNavHeight }, "levelselect_next", MenuAction::GuideNextPage, false });
    }
    buttons.push_back({ BackButtonBounds(), "guide_back", MenuAction::BackToMainMenu, false });
    return buttons;
}

MenuAction MenuScreen::UpdateGuide() {
    m_guidePage = std::clamp(m_guidePage, 0, kGuidePageCount - 1);

    // ESC / botón B cierran el glosario igual que el botón "Volver": es una
    // pantalla de consulta, no un menú del que haya que salir a propósito.
    // Quien decide A DÓNDE se vuelve es Application (menú principal o pausa,
    // según desde dónde se abriera) -- ver m_guideReturnTo.
    bool cancelPressed = IsKeyPressed(KEY_ESCAPE)
        || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT));
    if (cancelPressed) {
        PlayClickSound();
        return MenuAction::BackToMainMenu;
    }

    MenuAction action = UpdateButtonList(BuildGuideButtons());
    if (action == MenuAction::GuideNextPage) {
        m_guidePage++;
        return MenuAction::None;
    }
    if (action == MenuAction::GuidePrevPage) {
        m_guidePage--;
        return MenuAction::None;
    }
    return action;
}

void MenuScreen::DrawGuide(const UiContext& ui) const {
    const char* title = ui.localization.GetText("guide_title");
    constexpr float titleSize = LocalizationManager::kFontSizeTitle;
    Vector2 titleDim = MeasureTextEx(ui.localization.GetFontForSize(titleSize), title, titleSize, 1.0f);
    DrawTextEx(ui.localization.GetFontForSize(titleSize), title, Vector2{ (GetScreenWidth() - titleDim.x) / 2.0f, 80.0f }, titleSize, 1.0f, RAYWHITE);

    // Categoría de la página, entre el título (termina en 130) y la primera
    // fila (190): centrada a 145 con 24px, así que ocupa 145-169. Sin pisar
    // ninguna de las dos.
    int page = std::clamp(m_guidePage, 0, kGuidePageCount - 1);
    constexpr float categorySize = LocalizationManager::kFontSizeBody;
    const Font& categoryFont = ui.localization.GetFontForSize(categorySize);
    std::string category = std::string(ui.localization.GetText(GuidePageTitleKey(page))) +
                            TextFormat("   (%d/%d)", page + 1, kGuidePageCount);
    Vector2 categoryDim = MeasureTextEx(categoryFont, category.c_str(), categorySize, 1.0f);
    DrawTextEx(categoryFont, category.c_str(), Vector2{ (GetScreenWidth() - categoryDim.x) / 2.0f, 145.0f }, categorySize, 1.0f, SKYBLUE);

    float screenCenter = static_cast<float>(GetScreenWidth()) / 2.0f;
    float iconX = screenCenter - 450.0f;
    float nameX = screenCenter - 395.0f;
    float descX = screenCenter - 160.0f;

    constexpr float nameSize = LocalizationManager::kFontSizeBody;
    constexpr float descSize = LocalizationManager::kFontSizeControlsRow;
    const Font& nameFont = ui.localization.GetFontForSize(nameSize);
    const Font& descFont = ui.localization.GetFontForSize(descSize);

    const std::vector<GuideEntry>& entries = GuidePage(page);
    for (size_t i = 0; i < entries.size(); i++) {
        float rowY = kGuideFirstRowY + static_cast<float>(i) * kGuideRowHeight;

        DrawGuideIcon(entries[i].icon,
                       Rectangle{ iconX, rowY + (kGuideRowHeight - kGuideIconSize) / 2.0f - 6.0f, kGuideIconSize, kGuideIconSize });

        // Nombre y descripción centrados verticalmente cada uno respecto a su
        // propio alto, que no es el mismo (24 vs 22).
        DrawTextEx(nameFont, ui.localization.GetText(entries[i].nameKey),
                   Vector2{ nameX, rowY + (kGuideIconSize - nameSize) / 2.0f - 6.0f }, nameSize, 1.0f, GOLD);
        DrawTextEx(descFont, ui.localization.GetText(entries[i].descriptionKey),
                   Vector2{ descX, rowY + (kGuideIconSize - descSize) / 2.0f - 6.0f }, descSize, 1.0f, RAYWHITE);
    }

    DrawButtonList(BuildGuideButtons(), ui);
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
