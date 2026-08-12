#pragma once
#include "raylib.h"

// Caja de colisión temporal generada por un ataque (por ahora solo el
// Player la genera; Enemy::ATTACK puede reusar el mismo tipo más adelante).
// CombatSystem es quien la crea, la testea contra los Actor y la descarta.
struct Hitbox {
    BoundingBox box{};
    float damage = 0.0f;
    Vector3 knockbackDir{};
    float remainingTime = 0.15f; // ventana activa del golpe, en segundos
};
