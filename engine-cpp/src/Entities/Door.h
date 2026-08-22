#pragma once
#include "Entity.h"

// Objetivo final del nivel: al llegar el jugador aquí, se gana.
class Door : public Entity {
public:
    Door(Vector3 position, Vector3 halfExtents);

    void Update(float) override {}
    void Draw() const override;
};
