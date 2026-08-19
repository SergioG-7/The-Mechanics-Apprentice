#pragma once
#include "raylib.h"
#include "UiContext.h"
#include "../Core/SaveManager.h"
#include <string>
#include <vector>

// Qué botón pulsó el usuario, o None si ninguno este frame. MenuScreen no
// conoce AppState ni Application -- solo dibuja y hace hit-test/navegación
// de sus propios botones; es Application quien decide qué hacer con la
// acción devuelta (cambiar de AppState, cargar un nivel, guardar, etc.).
enum class MenuAction {
    None,
    OpenLevelSelect, // "Modo Historia" en el menú principal ya no arranca directo: abre el selector
    StartStory,      // disparado por un botón de nivel dentro de LevelSelect (ver MenuButton::levelNumber)
    StartEndless,
    OpenOptions,
    OpenControls,
    OpenStats,
    BackToMainMenu,
    BackToOptions,
    CycleLanguage,
    Quit
};

// Encapsula layout, dibujado, clics y navegación por teclado/mando de las
// cuatro pantallas de menú (principal, opciones, controles, estadísticas),
// para que Application no cargue con coordenadas de botones ni lógica de
// hit-test además de la orquestación de estados.
class MenuScreen {
public:
    MenuAction UpdateMainMenu();
    void DrawMainMenu(const UiContext& ui) const;

    MenuAction UpdateOptions();
    void DrawOptions(const UiContext& ui) const;

    MenuAction UpdateControls();
    void DrawControls(const UiContext& ui) const;

    MenuAction UpdateStats();
    void DrawStats(const UiContext& ui, const SaveData& saveData) const;

    // maxUnlockedLevel viene de SaveManager::Data().maxLevelUnlocked (MenuScreen
    // no conoce SaveManager, igual que DrawStats recibe SaveData ya resuelto).
    // Solo se listan botones 1..min(maxUnlockedLevel, kStoryLevelCount): no hay
    // niveles bloqueados-pero-visibles, un nivel no desbloqueado no aparece.
    MenuAction UpdateLevelSelect(int maxUnlockedLevel);
    void DrawLevelSelect(const UiContext& ui, int maxUnlockedLevel) const;

    // Nivel del botón que acaba de disparar MenuAction::StartStory desde
    // LevelSelect. Application lo lee justo después de recibir esa acción.
    int GetSelectedLevel() const { return m_lastSelectedLevel; }

    static constexpr int kStoryLevelCount = 5;

private:
    struct MenuButton {
        Rectangle bounds;
        const char* labelKey;
        MenuAction action;
        bool isLanguageButton = false; // compone el label con el idioma activo (ver DrawButtonList)
        int levelNumber = 0;           // > 0 solo en los botones de LevelSelect (ver DrawButtonList/UpdateButtonList)
    };

    std::vector<MenuButton> BuildMainMenuButtons() const;
    std::vector<MenuButton> BuildOptionsButtons() const;
    std::vector<MenuButton> BuildLevelSelectButtons(int maxUnlockedLevel) const;

    // Navegación compartida por las cuatro pantallas: flechas/WASD, D-Pad o
    // el stick izquierdo mueven m_selectedIndex; Enter o el botón de
    // "atacar" del mando activan el botón resaltado. Así el menú entero es
    // navegable sin ratón. Mover el ratón sobre un botón también lo
    // selecciona, para que ratón y mando no se desincronicen visualmente.
    MenuAction UpdateButtonList(const std::vector<MenuButton>& buttons);
    void DrawButtonList(const std::vector<MenuButton>& buttons, const UiContext& ui) const;
    void DrawButton(Rectangle bounds, const char* label, bool selected, const UiContext& ui) const;

    static bool IsButtonClicked(Rectangle bounds);
    static Rectangle StackedButton(int index, int count);
    static Rectangle VolumeSliderBounds();
    static Rectangle BackButtonBounds();
    static const char* NativeLanguageName(const std::string& code);

    int m_selectedIndex = 0;
    int m_lastSelectedLevel = 1;

    // Cooldown del eje analógico: sin él, mantener el stick empujado
    // recorrería toda la lista en un solo frame (60 veces por segundo). Los
    // botones (D-Pad, teclado) no lo necesitan -- IsGamepadButtonPressed /
    // IsKeyPressed ya son de flanco, no de nivel.
    float m_navCooldown = 0.0f;
};
