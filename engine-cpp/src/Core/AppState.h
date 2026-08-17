#pragma once

// Estado de más alto nivel: qué pantalla ve el jugador. GameState (ver
// GameState.h) vive un nivel más abajo, dentro de StoryMode/EndlessMode,
// para saber si la partida en curso sigue viva, se perdió o se ganó.
enum class AppState {
    MainMenu,
    Options,
    StoryMode,
    EndlessMode
};
