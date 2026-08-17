#include "ExplosiveBarrel.h"
#include "raylib.h"

ExplosiveBarrel::ExplosiveBarrel(Vector3 position, float maxHP)
    : Actor(position, maxHP, Vector3{ 0.45f, 0.6f, 0.45f }) {}

void ExplosiveBarrel::Draw() const {
    Vector3 base{ m_position.x, m_position.y - m_halfExtents.y, m_position.z };
    float height = m_halfExtents.y * 2.0f;
    DrawCylinder(base, m_halfExtents.x, m_halfExtents.x, height, 12, RED);
    DrawCylinderWires(base, m_halfExtents.x, m_halfExtents.x, height, 12, MAROON);
}

void ExplosiveBarrel::TakeDamage(float amount, Vector3 knockbackDir) {
    if (m_exploded) return;
    Actor::TakeDamage(amount, knockbackDir);
    if (!IsAlive()) m_exploded = true;
}
