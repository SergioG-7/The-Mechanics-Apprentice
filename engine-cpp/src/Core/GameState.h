#pragma once

// Estado de la partida en curso -- solo tiene sentido mientras AppState (ver
// AppState.h) está en StoryMode o EndlessMode. "Menu" ya no vive aquí: lo
// cubre AppState::MainMenu, que es quien de verdad decide qué pantalla ve
// el jugador.
enum class GameState {
    Gameplay,
    GameOver,
    Victory
};
