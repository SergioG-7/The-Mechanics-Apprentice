#pragma once
#include "raylib.h"

// Charco de lodo que deja el enemigo Trapper por donde pasa. Ralentiza al jugador que lo pisa.
struct MudPuddle {
    Vector3 position{};
    float lifetime = kLifetime;

    static constexpr float kRadius = 1.3f;
    static constexpr float kLifetime = 8.0f;
    static constexpr float kSlowDuration = 2.0f;
    static constexpr float kSlowMultiplier = 0.5f;
};
