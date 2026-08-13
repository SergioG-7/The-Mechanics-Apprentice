#pragma once
#include "Entity.h"

// Objetivo de recolección. Sin HP ni FSM -- hereda de Entity, no de Actor,
// igual que Obstacle. Application lo elimina de la lista al recogerlo.
class Gear : public Entity {
public:
    explicit Gear(Vector3 position);

    void Update(float) override {}
    void Draw() const override;
};
