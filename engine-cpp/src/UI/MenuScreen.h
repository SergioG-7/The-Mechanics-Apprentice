#pragma once
#include "raylib.h"

// Qué botón pulsó el usuario, o None si ninguno este frame. MenuScreen no
// conoce AppState ni Application -- solo dibuja y hace hit-test de sus
// propios botones; es Application quien decide qué hacer con la acción
// devuelta (cambiar de AppState, cargar un nivel, cerrar la ventana...).
enum class MenuAction {
    None,
    StartStory,
    StartEndless,
    OpenOptions,
    BackToMainMenu,
    Quit
};

// Encapsula el layout, dibujado y clics del menú principal y la pantalla de
// opciones, para que Application no cargue con coordenadas de botones ni
// lógica de hit-test además de la orquestación de estados.
class MenuScreen {
public:
    MenuAction UpdateMainMenu() const;
    void DrawMainMenu() const;

    // El volumen se lee/escribe directamente con Get/SetMasterVolume de
    // raylib: no hay estado de audio que Application necesite pasarle.
    MenuAction UpdateOptions() const;
    void DrawOptions() const;

private:
    static Rectangle StoryButtonBounds();
    static Rectangle EndlessButtonBounds();
    static Rectangle OptionsButtonBounds();
    static Rectangle QuitButtonBounds();
    static Rectangle BackButtonBounds();
    static Rectangle VolumeSliderBounds();

    static bool IsButtonClicked(Rectangle bounds);
    static void DrawButton(Rectangle bounds, const char* label);
};
