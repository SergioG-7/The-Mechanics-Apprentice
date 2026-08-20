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
    OpenGuide,       // glosario de mecánicas/enemigos/power-ups (botón "Guía" del menú principal)
    OpenLevelEditor, // botón "Editor de Niveles" anclado en la esquina superior izquierda del menú principal -- Application::LaunchLevelEditor, igual que F12
    BackToMainMenu,
    BackToOptions,
    CycleLanguage,
    ResumeGame, // botón "Continuar" del menú de pausa
    LevelSelectNextPage, // consumida dentro de MenuScreen::UpdateLevelSelect, nunca llega a Application
    LevelSelectPrevPage,
    GuideNextPage,       // consumidas dentro de MenuScreen::UpdateGuide, igual que las de LevelSelect
    GuidePrevPage,
    Quit
};

// Encapsula layout, dibujado, clics y navegación por teclado/mando de las
// cuatro pantallas de menú (principal, opciones, controles, estadísticas),
// para que Application no cargue con coordenadas de botones ni lógica de
// hit-test además de la orquestación de estados.
class MenuScreen {
public:
    MenuScreen() = default;
    ~MenuScreen();

    // Carga el SFX de clic de UI -- aparte del constructor a propósito:
    // necesita InitAudioDevice() ya llamado, y MenuScreen es un miembro por
    // VALOR de Application (se construye antes de que el cuerpo del
    // constructor de Application llegue a InitAudioDevice()), a diferencia
    // de m_music/m_toonShader, que son unique_ptr construidos ahí mismo.
    void LoadSfx();

    // Reaplica AudioSettings::GetSfxVolume() al Sound ya cargado -- ver
    // Player::RefreshSfxVolume, mismo motivo.
    void RefreshSfxVolume();

    MenuAction UpdateMainMenu();
    void DrawMainMenu(const UiContext& ui) const;

    // bgmVolume/sfxVolume por referencia: el arrastre del slider los escribe
    // directamente (Application los tiene en SaveManager::Data(), no hace
    // falta que MenuScreen conozca SaveManager para poder tocarlos).
    MenuAction UpdateOptions(float& bgmVolume, float& sfxVolume);
    void DrawOptions(const UiContext& ui, float bgmVolume, float sfxVolume) const;

    // Menú superpuesto durante una partida en pausa (ESC) -- Application
    // sigue dibujando la escena 3D y el HUD detrás (ver DrawGameplay), esto
    // solo pinta el overlay oscuro + los tres botones encima.
    MenuAction UpdatePause();
    void DrawPause(const UiContext& ui) const;

    MenuAction UpdateControls();
    void DrawControls(const UiContext& ui) const;

    MenuAction UpdateStats();
    void DrawStats(const UiContext& ui, const SaveData& saveData) const;

    // Glosario paginado por categoría (Mecánicas / Enemigos / Power-ups): una
    // página por categoría, con icono dibujado a mano, nombre y descripción
    // localizados. Los iconos son primitivas 2D, no un render 3D de la
    // entidad real: mantienen su color y su silueta característicos sin
    // arrastrar cámara, shader ni modelos a una pantalla de menú.
    MenuAction UpdateGuide();
    void DrawGuide(const UiContext& ui) const;

    // maxUnlockedLevel viene de SaveManager::Data().maxLevelUnlocked (MenuScreen
    // no conoce SaveManager, igual que DrawStats recibe SaveData ya resuelto).
    // Solo se listan botones 1..maxUnlockedLevel: no hay niveles
    // bloqueados-pero-visibles, un nivel no desbloqueado no aparece. Paginado
    // de kLevelsPerPage en kLevelsPerPage -- antes tenía un límite fijo de 5
    // niveles en total (kStoryLevelCount), así que un maxUnlockedLevel mayor
    // ni siquiera se podía seleccionar por aquí.
    MenuAction UpdateLevelSelect(int maxUnlockedLevel);
    void DrawLevelSelect(const UiContext& ui, int maxUnlockedLevel) const;

    // Nivel del botón que acaba de disparar MenuAction::StartStory desde
    // LevelSelect. Application lo lee justo después de recibir esa acción.
    int GetSelectedLevel() const { return m_lastSelectedLevel; }

private:
    struct MenuButton {
        Rectangle bounds;
        const char* labelKey;
        MenuAction action;
        bool isLanguageButton = false; // compone el label con el idioma activo (ver DrawButtonList)
        int levelNumber = 0;           // > 0 solo en los botones de LevelSelect (ver DrawButtonList/UpdateButtonList)
    };

    // Qué icono dibuja una fila del glosario. No hay un enum de "tipo de
    // entidad" compartido con el motor (Gear, Enemy... no lo necesitan), así
    // que el glosario lleva el suyo, mínimo y local.
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

    // Dibuja el icono de una fila dentro de un cuadrado de kGuideIconSize.
    static void DrawGuideIcon(GuideIcon icon, Rectangle bounds);

    std::vector<MenuButton> BuildMainMenuButtons() const;
    std::vector<MenuButton> BuildOptionsButtons() const;
    std::vector<MenuButton> BuildPauseButtons() const;
    std::vector<MenuButton> BuildLevelSelectButtons(int maxUnlockedLevel) const;
    std::vector<MenuButton> BuildGuideButtons() const;

    // Navegación compartida por las cuatro pantallas: flechas/WASD, D-Pad o
    // el stick izquierdo mueven m_selectedIndex; Enter o el botón de
    // "atacar" del mando activan el botón resaltado. Así el menú entero es
    // navegable sin ratón. Mover el ratón sobre un botón también lo
    // selecciona, para que ratón y mando no se desincronicen visualmente.
    MenuAction UpdateButtonList(const std::vector<MenuButton>& buttons);
    void DrawButtonList(const std::vector<MenuButton>& buttons, const UiContext& ui) const;
    void DrawButton(Rectangle bounds, const char* label, bool selected, const UiContext& ui) const;
    void PlayClickSound() const;

    // Compartidos por los dos sliders de volumen (Música/Efectos) de
    // Opciones -- un solo sitio para el dibujado y otro para el arrastre,
    // en vez de duplicar cada uno dos veces.
    void DrawVolumeSlider(const UiContext& ui, Rectangle bounds, const char* labelKey, float volume) const;
    static void UpdateVolumeSliderDrag(Rectangle bounds, float& volume);

    static bool IsButtonClicked(Rectangle bounds);
    static Rectangle StackedButton(int index, int count);

    // Y del primer botón de una lista apilada de 'count' elementos: centrada
    // en pantalla, PERO nunca por encima de kMenuContentTop. Con los 6
    // botones del menú principal (el de Guía es el sexto) el centrado puro
    // daba startY = 130, justo dentro del título, que llega hasta los 150 --
    // los dos se pisaban. Al pasarse del tope, la lista se ancla ahí y crece
    // hacia abajo; con 3 botones (pausa) sigue quedando centrada como antes.
    static float StackedListStartY(int count);

    // Primer píxel utilizable bajo el título de una pantalla de menú: el
    // título se dibuja a y=100 con kFontSizeTitle (50), así que termina sobre
    // los 150; 170 deja 20px de aire.
    static constexpr float kMenuContentTop = 170.0f;
    static Rectangle BgmVolumeSliderBounds();
    static Rectangle SfxVolumeSliderBounds();
    static Rectangle BackButtonBounds();

    // Selector de nivel: botones de nivel apilados verticalmente con
    // StackedButton, igual que el resto del menú -- pero con un número
    // VIRTUAL de filas fijo (kLevelSelectVirtualRows), no el recuento real
    // de botones de esta página en concreto. Si se usara el recuento real,
    // la posición vertical de la lista saltaría entre páginas (una página
    // con Prev+Next ocupa una fila más que una sin ninguno de los dos), y
    // con Prev+Next+5 niveles+Volver a la vez el conjunto se saldría de
    // pantalla por arriba. Con el número fijo, la fila de Anterior/Siguiente
    // (compartida, partida en dos mitades) siempre cae en la misma Y sea
    // cual sea la página, y Volver usa su posición fija de siempre
    // (BackButtonBounds), igual que en Controles/Estadísticas.
    static Rectangle LevelSelectNavButtonBounds(bool isNext);

    // Botón "Editor de Niveles": anclado a la esquina superior izquierda,
    // fuera del flujo vertical de StackedButton -- con 6 botones apilados
    // (los 5 de siempre + este) el centrado vertical automático empujaba el
    // primero hacia arriba hasta pisar el título ("THE MECHANIC'S
    // APPRENTICE", ver DrawMainMenu). Solo clicable con ratón (no forma
    // parte de UpdateButtonList), F12 sigue siendo el acceso por
    // teclado/mando -- ver MenuAction::OpenLevelEditor.
    static Rectangle EditorButtonBounds();

    static constexpr int kLevelsPerPage = 5;
    static constexpr int kLevelSelectVirtualRows = 6; // 5 niveles + 1 fila de Anterior/Siguiente

    int m_selectedIndex = 0;
    int m_lastSelectedLevel = 1;
    int m_levelSelectPage = 0; // 0-indexado; ver UpdateLevelSelect/BuildLevelSelectButtons
    int m_guidePage = 0;       // 0-indexado, una página por categoría; ver UpdateGuide

    // Cooldown del eje analógico: sin él, mantener el stick empujado
    // recorrería toda la lista en un solo frame (60 veces por segundo). Los
    // botones (D-Pad, teclado) no lo necesitan -- IsGamepadButtonPressed /
    // IsKeyPressed ya son de flanco, no de nivel.
    float m_navCooldown = 0.0f;

    Sound m_clickSound{};
};
