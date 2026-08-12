#pragma once
#include "raylib.h"
#include <vector>
#include <memory>

// Base para todo lo que el LevelLoader puede instanciar y que ocupa espacio
// en el mundo: Player, Enemy, Obstacle. Solo maneja posición y colisión —
// HP y daño viven un nivel más abajo, en Actor, porque Obstacle no los necesita.
class Entity {
public:
    explicit Entity(Vector3 position, Vector3 halfExtents = { 0.5f, 0.5f, 0.5f });
    virtual ~Entity() = default;

    virtual void Update(float dt) = 0;
    virtual void Draw() const = 0;

    Vector3 GetPosition() const { return m_position; }
    virtual BoundingBox GetBoundingBox() const;

protected:
    // Intenta desplazar la entidad por delta (X y Z se resuelven por
    // separado a propósito: si un eje choca contra un Obstacle y el otro no,
    // el movimiento en el eje libre se sigue aplicando -> "desliza" a lo
    // largo del obstáculo en vez de quedarse parado en seco). Usado por
    // Player/Enemy en su Update(); Obstacle nunca lo llama.
    void TryMove(Vector3 delta, const std::vector<std::unique_ptr<Entity>>& obstacles);

    Vector3 m_position{};
    Vector3 m_halfExtents;

private:
    bool CollidesWithAny(Vector3 candidatePosition, const std::vector<std::unique_ptr<Entity>>& obstacles) const;
};
