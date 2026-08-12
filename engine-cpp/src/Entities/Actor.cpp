#include "Actor.h"
#include "../Combat/CollisionMath.h"
#include <algorithm>

Actor::Actor(Vector3 position, float maxHP, Vector3 halfExtents)
    : Entity(position, halfExtents), m_hp(maxHP), m_maxHP(maxHP) {}

void Actor::TakeDamage(float amount, Vector3 knockbackDir) {
    if (!IsAlive()) return;
    m_hp = std::max(0.0f, m_hp - amount);
    m_knockbackVelocity = knockbackDir;
}

void Actor::ApplyKnockback(float dt) {
    m_position.x += m_knockbackVelocity.x * dt;
    m_position.y += m_knockbackVelocity.y * dt;
    m_position.z += m_knockbackVelocity.z * dt;

    float decay = 1.0f - std::min(1.0f, kKnockbackDrag * dt);
    m_knockbackVelocity.x *= decay;
    m_knockbackVelocity.y *= decay;
    m_knockbackVelocity.z *= decay;
}

bool Actor::WouldCollideWithObstacles(Vector3 candidatePosition) const {
    if (!m_obstacles) return false;

    BoundingBox candidateBox{
        Vector3{ candidatePosition.x - m_halfExtents.x, candidatePosition.y - m_halfExtents.y, candidatePosition.z - m_halfExtents.z },
        Vector3{ candidatePosition.x + m_halfExtents.x, candidatePosition.y + m_halfExtents.y, candidatePosition.z + m_halfExtents.z }
    };

    for (const auto& obstacle : *m_obstacles) {
        if (CollisionMath::AABBIntersects(candidateBox, obstacle->GetBoundingBox())) {
            return true;
        }
    }
    return false;
}
