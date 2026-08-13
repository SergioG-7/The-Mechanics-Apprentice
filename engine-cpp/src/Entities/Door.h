#pragma once
#include "Entity.h"

// Objetivo final del nivel: posición + tamaño configurables desde el editor,
// igual que Obstacle. Sin lógica de apertura/victoria todavía.
class Door : public Entity {
public:
    Door(Vector3 position, Vector3 halfExtents);

    void Update(float) override {}
    void Draw() const override;
};
