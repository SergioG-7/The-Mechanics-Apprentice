#include "Cylinder.h"
#include "raylib.h"

Cylinder::Cylinder(Vector3 position, float radius, float height)
    : Entity(position, Vector3{ radius, height * 0.5f, radius }), m_radius(radius), m_height(height) {}

void Cylinder::Draw() const {
    Vector3 base{ m_position.x, m_position.y - m_halfExtents.y, m_position.z };
    DrawCylinder(base, m_radius, m_radius, m_height, 16, DARKGRAY);
    DrawCylinderWires(base, m_radius, m_radius, m_height, 16, SKYBLUE); // mismo contorno holográfico que Obstacle
}
