#pragma once

// Qué pantalla está viendo el jugador ahora mismo.
enum class AppState {
    MainMenu,
    Options,
    Controls,    
    Stats,       
    Guide,       
    LevelSelect, 
    StoryMode,
    EndlessMode,
    Paused 
};
