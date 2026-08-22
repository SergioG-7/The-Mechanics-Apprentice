#pragma once

// Estado de la partida en curso, mientras se está jugando en Modo Historia o Endless.
enum class GameState {
    WaitingToStart, // nivel cargado pero enemigos y trampas aún no se mueven
    Gameplay,
    GameOver,
    Victory
};
