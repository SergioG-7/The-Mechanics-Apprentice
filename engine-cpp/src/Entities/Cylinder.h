#pragma once
#include "Entity.h"

// Pilar cilíndrico estático que bloquea el movimiento, como Obstacle pero redondo.
class Cylinder : public Entity {
public:
    Cylinder(Vector3 position, float radius, float height);

    void Update(float) override {}
    void Draw() const override;

private:
    float m_radius;
    float m_height;
};
