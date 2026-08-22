#include "HealthKit.h"
#include "raylib.h"

HealthKit::HealthKit(Vector3 position, float healAmount)
    : Entity(position, Vector3{ 0.35f, 0.35f, 0.35f }), m_healAmount(healAmount) {}

void HealthKit::Draw() const {
    float size = m_halfExtents.x * 2.0f;
    DrawCube(m_position, size, size, size, GREEN);
    DrawCubeWires(m_position, size, size, size, DARKGREEN);

    // Cruz blanca encima, hecha con dos cubos finos cruzados.
    constexpr float armLength = 0.5f;
    constexpr float armThickness = 0.12f;
    Vector3 top{ m_position.x, m_position.y + m_halfExtents.y + 0.01f, m_position.z };
    DrawCube(top, armLength, 0.02f, armThickness, RAYWHITE);
    DrawCube(top, armThickness, 0.02f, armLength, RAYWHITE);
}
