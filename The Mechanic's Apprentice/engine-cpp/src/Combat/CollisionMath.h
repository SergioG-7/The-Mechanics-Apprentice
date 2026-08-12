#pragma once
#include "raylib.h"

namespace CollisionMath {
    bool AABBIntersects(const BoundingBox& a, const BoundingBox& b);

    float DistanceSquared(Vector3 a, Vector3 b);
    bool IsWithinRadius(Vector3 a, Vector3 b, float radius);

    // Normaliza en el plano XZ (Y se descarta a propósito: el juego es 2D
    // de movimiento sobre el suelo). Player y Enemy la comparten para no
    // duplicar la misma normalización a mano en los dos .cpp.
    Vector3 Normalize2D(Vector3 v);
}
