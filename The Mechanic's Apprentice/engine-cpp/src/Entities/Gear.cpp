#include "Gear.h"
#include "raylib.h"

Gear::Gear(Vector3 position)
    : Entity(position, Vector3{ 0.3f, 0.3f, 0.3f }) {} // más pequeño que Player/Enemy/Obstacle por defecto

void Gear::Draw() const {
    DrawCube(m_position, m_halfExtents.x * 2.0f, m_halfExtents.y * 2.0f, m_halfExtents.z * 2.0f, GOLD);
}
