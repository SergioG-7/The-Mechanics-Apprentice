#pragma once

// Menu queda reservado para cuando exista un menú real (fuera del alcance
// de esta fase); Application arranca directo en Gameplay. GameOver/Victory
// sustituyen al "End" genérico original de la Fase 1 -- son los dos
// desenlaces reales que ya tiene el juego.
enum class GameState {
    Menu,
    Gameplay,
    GameOver,
    Victory
};
