#pragma once
#include "raylib.h"

// Proyectil que dispara el enemigo Spitter: vuelo recto, sin gravedad.
struct Projectile {
    Vector3 position{};
    Vector3 velocity{};
    float damage = 0.0f;
    float lifetime = 3.0f; // se destruye solo si no impacta a tiempo

    static constexpr float kRadius = 0.2f;
};
