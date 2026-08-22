#pragma once
#include "Entity.h"

// Engranaje que el jugador recoge como objetivo del nivel.
class Gear : public Entity {
public:
    explicit Gear(Vector3 position);

    void Update(float) override {}
    void Draw() const override;
};
