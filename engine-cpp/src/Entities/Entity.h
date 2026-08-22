#pragma once
#include "raylib.h"
#include <vector>
#include <memory>

// Base de todo lo que ocupa espacio en el mundo: Player, Enemy, Obstacle. Solo maneja posición y colisión.
class Entity {
public:
    explicit Entity(Vector3 position, Vector3 halfExtents = { 0.5f, 0.5f, 0.5f });
    virtual ~Entity() = default;

    virtual void Update(float dt) = 0;
    virtual void Draw() const = 0;

    Vector3 GetPosition() const { return m_position; }
    virtual BoundingBox GetBoundingBox() const;

protected:
    // Intenta mover la entidad, deslizando a lo largo de los obstáculos en vez de bloquearse en seco.
    void TryMove(Vector3 delta, const std::vector<std::unique_ptr<Entity>>& obstacles);

    Vector3 m_position{};
    Vector3 m_halfExtents;

private:
    bool CollidesWithAny(Vector3 candidatePosition, const std::vector<std::unique_ptr<Entity>>& obstacles) const;
};
