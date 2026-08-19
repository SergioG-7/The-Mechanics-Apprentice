#pragma once
#include "Entity.h"

// Pilar cilíndrico estático -- mismo rol que Obstacle (bloquea movimiento,
// sin HP), pero redondo. La colisión sigue siendo el AABB genérico que ya
// resuelve Entity::TryMove (aproximación cilíndrica sólida: un cuadrado de
// lado 2*radius circunscrito al círculo), así que vive en la MISMA lista
// que Obstacle (LevelData::obstacles, std::vector<unique_ptr<Entity>>) sin
// tocar el código de colisión -- solo cambia Draw().
class Cylinder : public Entity {
public:
    Cylinder(Vector3 position, float radius, float height);

    void Update(float) override {}
    void Draw() const override;

private:
    float m_radius;
    float m_height;
};
