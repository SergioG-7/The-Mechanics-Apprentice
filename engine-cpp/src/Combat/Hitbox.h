#pragma once
#include "raylib.h"

// Caja de colisión temporal que genera un ataque de Player o Enemy.
struct Hitbox {
    BoundingBox box{};
    float damage = 0.0f;
    Vector3 knockbackDir{};
    float remainingTime = 0.15f; // segundos que queda activa
};
