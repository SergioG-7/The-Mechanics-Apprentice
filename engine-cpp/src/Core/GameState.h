#pragma once

// Estado de la partida en curso -- solo tiene sentido mientras AppState (ver
// AppState.h) está en StoryMode o EndlessMode. "Menu" ya no vive aquí: lo
// cubre AppState::MainMenu, que es quien de verdad decide qué pantalla ve
// el jugador.
enum class GameState {
    // Nivel recién cargado (o reintentado): el jugador puede moverse, pero
    // enemigos/trampas/spawners no se actualizan hasta que pulsa Atacar --
    // ver Application::UpdateWaitingToStart. Da un respiro antes de que
    // empiece a moverse nada.
    WaitingToStart,
    Gameplay,
    GameOver,
    Victory
};
