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

    // Línea de visión en el plano XZ (Y se ignora, igual que Normalize2D):
    // true si el segmento start->end atraviesa box. Método de las franjas
    // (slab test) recortado a 2D. La usa CombatSystem::ResolveMeleeAttack
    // para descartar un golpe cuerpo a cuerpo si un Obstacle se interpone
    // entre el jugador y el enemigo, aunque ambos sigan dentro del alcance.
    bool SegmentIntersectsBoxXZ(Vector3 start, Vector3 end, const BoundingBox& box);
}
