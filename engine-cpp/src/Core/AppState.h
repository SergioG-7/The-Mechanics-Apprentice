#pragma once

// Estado de más alto nivel: qué pantalla ve el jugador. GameState (ver
// GameState.h) vive un nivel más abajo, dentro de StoryMode/EndlessMode,
// para saber si la partida en curso sigue viva, se perdió o se ganó.
enum class AppState {
    MainMenu,
    Options,
    Controls,    // se abre desde Options; su "Volver" regresa a Options, no al menú principal
    Stats,       // se abre desde MainMenu; su "Volver" regresa al menú principal
    LevelSelect, // se abre desde MainMenu ("Modo Historia"); su "Volver" regresa al menú principal
    StoryMode,
    EndlessMode
};
