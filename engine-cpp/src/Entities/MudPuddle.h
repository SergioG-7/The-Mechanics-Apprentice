#pragma once
#include "raylib.h"

// Charco de lodo ácido que deja un Trapper por donde pasa. Struct plano, no
// Entity: igual que Projectile, no tiene dueño propio (nace cuando
// Enemy::ConsumePendingPuddle devuelve una posición y muere al agotarse su
// lifetime) y no bloquea el paso -- solo ralentiza al Player que lo pisa.
// Ver CombatSystem::UpdateMudPuddles.
struct MudPuddle {
    Vector3 position{};
    float lifetime = kLifetime;

    static constexpr float kRadius = 1.3f;
    static constexpr float kLifetime = 8.0f;
    static constexpr float kSlowDuration = 2.0f;
    static constexpr float kSlowMultiplier = 0.5f;
};
