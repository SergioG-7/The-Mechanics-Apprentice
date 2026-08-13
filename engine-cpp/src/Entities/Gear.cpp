#include "Gear.h"
#include "raylib.h"
#include "rlgl.h"

Gear::Gear(Vector3 position)
    : Entity(position, Vector3{ 0.3f, 0.3f, 0.3f }) {} // más pequeño que Player/Enemy/Obstacle por defecto

void Gear::Draw() const {
    constexpr float kRotationSpeedDegPerSec = 90.0f;
    constexpr int kSlices = 6; // silueta de tuerca hexagonal, encaja con el tema mecánico

    // rlPushMatrix/rlRotatef en vez de un ángulo por parámetro: DrawCylinder
    // no tiene overload con rotación, así que se rota la matriz de mundo
    // igual que raylib hace internamente en sus propias funciones Draw*.
    rlPushMatrix();
    rlTranslatef(m_position.x, m_position.y, m_position.z);
    rlRotatef((float)GetTime() * kRotationSpeedDegPerSec, 0.0f, 1.0f, 0.0f);

    Vector3 base{ 0.0f, -m_halfExtents.y, 0.0f };
    float height = m_halfExtents.y * 2.0f;
    DrawCylinder(base, m_halfExtents.x, m_halfExtents.x, height, kSlices, GOLD);
    DrawCylinderWires(base, m_halfExtents.x, m_halfExtents.x, height, kSlices, YELLOW);

    rlPopMatrix();
}
