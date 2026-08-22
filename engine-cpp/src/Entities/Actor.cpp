#include "Actor.h"
#include <algorithm>

Actor::Actor(Vector3 position, float maxHP, Vector3 halfExtents)
    : Entity(position, halfExtents), m_hp(maxHP), m_maxHP(maxHP) {}

void Actor::TakeDamage(float amount, Vector3 knockbackDir) {
    if (!IsAlive()) return;
    m_hp = std::max(0.0f, m_hp - amount);
    m_knockbackVelocity = knockbackDir;
}

void Actor::Heal(float amount) {
    // Un actor muerto ya no se cura.
    if (!IsAlive()) return;
    m_hp = std::min(m_maxHP, m_hp + amount);
}

void Actor::ApplyKnockback(float dt) {
    TryMoveAgainstObstacles(Vector3{ m_knockbackVelocity.x * dt, 0.0f, m_knockbackVelocity.z * dt });
    m_position.y += m_knockbackVelocity.y * dt;

    float decay = 1.0f - std::min(1.0f, kKnockbackDrag * dt);
    m_knockbackVelocity.x *= decay;
    m_knockbackVelocity.y *= decay;
    m_knockbackVelocity.z *= decay;
}

void Actor::TryMoveAgainstObstacles(Vector3 delta) {
    if (!m_obstacles) return;
    TryMove(delta, *m_obstacles);
}
