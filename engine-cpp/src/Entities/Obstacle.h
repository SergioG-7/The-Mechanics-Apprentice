#pragma once
#include "Entity.h"

// Bloque estático e indestructible que bloquea el movimiento.
class Obstacle : public Entity {
public:
    Obstacle(Vector3 position, Vector3 halfExtents);

    void Update(float) override {}
    void Draw() const override;
};
