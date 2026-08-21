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

    // Dirección unitaria de 'from' a 'to' en el plano XZ. Sustituye al
    // patrón `Normalize2D(Vector3{ b.x - a.x, 0, b.z - a.z })`, que estaba
    // escrito a mano en ocho sitios distintos (Enemy, CombatSystem x5,
    // Application) y en el que era fácil colar los operandos al revés.
    Vector3 DirectionXZ(Vector3 from, Vector3 to);

    // Escala una dirección XZ por una magnitud, dejando Y a 0. Es el vector
    // de knockback que construían a mano ApplyAreaDamage, ApplyHazardDamage,
    // UpdateProjectiles, UpdateElectricTiles y las dos hitboxes cuerpo a
    // cuerpo, todos con la misma forma `{ dir.x * f, 0, dir.z * f }`.
    Vector3 ScaleXZ(Vector3 direction, float magnitude);

    // Ángulo de encaramiento en GRADOS para DrawModelEx/rlRotatef a partir de
    // una dirección XZ (atan2(x, z), es decir 0° mirando a +Z). Player::Draw y
    // Enemy::Draw repetían la misma conversión con su propia constante de
    // radianes a grados.
    float HeadingDegrees(Vector3 direction);

    // Línea de visión en el plano XZ (Y se ignora, igual que Normalize2D):
    // true si el segmento start->end atraviesa box. Método de las franjas
    // (slab test) recortado a 2D. La usa CombatSystem::ResolveMeleeAttack
    // para descartar un golpe cuerpo a cuerpo si un Obstacle se interpone
    // entre el jugador y el enemigo, aunque ambos sigan dentro del alcance.
    bool SegmentIntersectsBoxXZ(Vector3 start, Vector3 end, const BoundingBox& box);
}
