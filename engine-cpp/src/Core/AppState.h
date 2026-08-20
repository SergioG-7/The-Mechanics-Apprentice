#pragma once

// Estado de más alto nivel: qué pantalla ve el jugador. GameState (ver
// GameState.h) vive un nivel más abajo, dentro de StoryMode/EndlessMode,
// para saber si la partida en curso sigue viva, se perdió o se ganó.
enum class AppState {
    MainMenu,
    Options,
    Controls,    // se abre desde Options; su "Volver" regresa a Options, no al menú principal
    Stats,       // se abre desde MainMenu; su "Volver" regresa al menú principal
    Guide,       // glosario de mecánicas/enemigos/power-ups; se abre desde MainMenu, vuelve al menú principal
    LevelSelect, // se abre desde MainMenu ("Modo Historia"); su "Volver" regresa al menú principal
    StoryMode,
    EndlessMode,
    Paused // ESC durante StoryMode/EndlessMode; ver Application::m_pausedFromState
};
