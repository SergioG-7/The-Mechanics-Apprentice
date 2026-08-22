#pragma once
#include "raylib.h"
#include "UiContext.h"
#include "../Core/SaveManager.h"
#include <string>
#include <vector>

// Qué botón pulsó el usuario, o None si ninguno este frame.
enum class MenuAction {
    None,
    OpenLevelSelect,
    StartStory,
    StartEndless,
    OpenOptions,
    OpenControls,
    OpenStats,
    OpenGuide,
    OpenLevelEditor,
    BackToMainMenu,
    BackToOptions,
    CycleLanguage,
    ResumeGame,
    LevelSelectNextPage,
    LevelSelectPrevPage,
    GuideNextPage,
    GuidePrevPage,
    Quit
};

// Dibuja, gestiona clics y navegación por teclado/mando de las pantallas de menú.
class MenuScreen {
public:
    MenuScreen() = default;
    ~MenuScreen();

    // Carga el sonido de clic de la UI.
    void LoadSfx();

    // Actualiza el volumen del sonido de clic según los ajustes de audio actuales.
    void RefreshSfxVolume();

    MenuAction UpdateMainMenu();
    void DrawMainMenu(const UiContext& ui) const;

    MenuAction UpdateOptions(float& bgmVolume, float& sfxVolume);
    void DrawOptions(const UiContext& ui, float bgmVolume, float sfxVolume) const;

    // Menú superpuesto durante una partida en pausa.
    MenuAction UpdatePause();
    void DrawPause(const UiContext& ui) const;

    MenuAction UpdateControls();
    void DrawControls(const UiContext& ui) const;

    MenuAction UpdateStats();
    void DrawStats(const UiContext& ui, const SaveData& saveData) const;

    // Glosario paginado de mecánicas, enemigos y power-ups.
    MenuAction UpdateGuide();
    void DrawGuide(const UiContext& ui) const;

    // Selector de nivel: solo lista los niveles ya desbloqueados, paginados.
    MenuAction UpdateLevelSelect(int maxUnlockedLevel);
    void DrawLevelSelect(const UiContext& ui, int maxUnlockedLevel) const;

    // Nivel elegido en el último StartStory disparado desde el selector.
    int GetSelectedLevel() const { return m_lastSelectedLevel; }

private:
    struct MenuButton {
        Rectangle bounds;
        const char* labelKey;
        MenuAction action;
        bool isLanguageButton = false; // el label incluye el idioma activo
        int levelNumber = 0;           // > 0 solo en los botones de LevelSelect
    };

    // Qué icono dibuja una fila del glosario.
    enum class GuideIcon { Door, Gear, Barrel, HealthKit, Spikes, ElectricTile, MudPuddle,
                           EnemyMelee, EnemyRunner, EnemySpitter, EnemyKamikaze,
                           EnemyShielder, EnemyBuffer, EnemyTrapper,
                           PowerOverclock, PowerFrenzy, PowerShield };

    struct GuideEntry {
        GuideIcon icon;
        const char* nameKey;
        const char* descriptionKey;
    };

    // Las tres páginas del glosario, en el orden en que se muestran.
    static const std::vector<GuideEntry>& GuidePage(int page);
    static const char* GuidePageTitleKey(int page);
    static constexpr int kGuidePageCount = 3;

    // Dibuja el icono de una fila del glosario.
    static void DrawGuideIcon(GuideIcon icon, Rectangle bounds);

    std::vector<MenuButton> BuildMainMenuButtons() const;
    std::vector<MenuButton> BuildOptionsButtons() const;
    std::vector<MenuButton> BuildPauseButtons() const;
    std::vector<MenuButton> BuildLevelSelectButtons(int maxUnlockedLevel) const;
    std::vector<MenuButton> BuildGuideButtons() const;

    // Navegación compartida por teclado, mando y ratón entre los botones de una pantalla.
    MenuAction UpdateButtonList(const std::vector<MenuButton>& buttons);
    void DrawButtonList(const std::vector<MenuButton>& buttons, const UiContext& ui) const;
    void DrawButton(Rectangle bounds, const char* label, bool selected, const UiContext& ui) const;
    void PlayClickSound() const;

    // Dibujado y arrastre compartidos por los dos sliders de volumen de Opciones.
    void DrawVolumeSlider(const UiContext& ui, Rectangle bounds, const char* labelKey, float volume) const;
    static void UpdateVolumeSliderDrag(Rectangle bounds, float& volume);

    static bool IsButtonClicked(Rectangle bounds);
    static Rectangle StackedButton(int index, int count);

    // Y del primer botón de una lista apilada, centrada pero sin pisar el título.
    static float StackedListStartY(int count);

    // Primer píxel utilizable bajo el título de una pantalla de menú.
    static constexpr float kMenuContentTop = 170.0f;
    static Rectangle BgmVolumeSliderBounds();
    static Rectangle SfxVolumeSliderBounds();
    static Rectangle BackButtonBounds();

    // Posición de los botones de Anterior/Siguiente página del selector de nivel.
    static Rectangle LevelSelectNavButtonBounds(bool isNext);

    // Botón "Editor de Niveles", anclado en la esquina superior izquierda del menú principal.
    static Rectangle EditorButtonBounds();

    static constexpr int kLevelsPerPage = 5;
    static constexpr int kLevelSelectVirtualRows = 6; // 5 niveles + 1 fila de Anterior/Siguiente

    int m_selectedIndex = 0;
    int m_lastSelectedLevel = 1;
    int m_levelSelectPage = 0;
    int m_guidePage = 0;

    // Cooldown para que mantener el stick del mando no recorra la lista de golpe.
    float m_navCooldown = 0.0f;

    Sound m_clickSound{};
};
