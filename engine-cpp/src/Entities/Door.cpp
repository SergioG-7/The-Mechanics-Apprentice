#include "Door.h"
#include "raylib.h"

Door::Door(Vector3 position, Vector3 halfExtents)
    : Entity(position, halfExtents) {}

void Door::Draw() const {
    DrawCube(m_position, m_halfExtents.x * 2.0f, m_halfExtents.y * 2.0f, m_halfExtents.z * 2.0f, GREEN);
}
