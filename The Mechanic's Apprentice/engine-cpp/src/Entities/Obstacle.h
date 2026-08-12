#pragma once
#include "Entity.h"

// Estático e indestructible — por eso hereda de Entity y no de Actor
// (sin HP, sin TakeDamage). El tamaño de su AABB viene del JSON (halfExtents),
// no es fijo como en Player/Enemy.
class Obstacle : public Entity {
public:
    Obstacle(Vector3 position, Vector3 halfExtents);

    void Update(float dt) override {}
    void Draw() const override;
};
