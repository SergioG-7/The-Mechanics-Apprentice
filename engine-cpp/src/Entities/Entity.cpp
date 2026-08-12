#include "Entity.h"
#include "../Combat/CollisionMath.h"

Entity::Entity(Vector3 position, Vector3 halfExtents)
    : m_position(position), m_halfExtents(halfExtents) {}

BoundingBox Entity::GetBoundingBox() const {
    return BoundingBox{
        Vector3{ m_position.x - m_halfExtents.x, m_position.y - m_halfExtents.y, m_position.z - m_halfExtents.z },
        Vector3{ m_position.x + m_halfExtents.x, m_position.y + m_halfExtents.y, m_position.z + m_halfExtents.z }
    };
}

bool Entity::CollidesWithAny(Vector3 candidatePosition, const std::vector<std::unique_ptr<Entity>>& obstacles) const {
    BoundingBox candidateBox{
        Vector3{ candidatePosition.x - m_halfExtents.x, candidatePosition.y - m_halfExtents.y, candidatePosition.z - m_halfExtents.z },
        Vector3{ candidatePosition.x + m_halfExtents.x, candidatePosition.y + m_halfExtents.y, candidatePosition.z + m_halfExtents.z }
    };

    for (const auto& obstacle : obstacles) {
        if (CollisionMath::AABBIntersects(candidateBox, obstacle->GetBoundingBox())) {
            return true;
        }
    }
    return false;
}

void Entity::TryMove(Vector3 delta, const std::vector<std::unique_ptr<Entity>>& obstacles) {
    Vector3 candidate = m_position;
    candidate.x += delta.x;
    if (!CollidesWithAny(candidate, obstacles)) {
        m_position.x = candidate.x;
    }

    candidate = m_position; // usa la X ya resuelta como base para probar Z
    candidate.z += delta.z;
    if (!CollidesWithAny(candidate, obstacles)) {
        m_position.z = candidate.z;
    }
}
