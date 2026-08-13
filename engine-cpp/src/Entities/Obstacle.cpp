#include "Obstacle.h"
#include "raylib.h"

Obstacle::Obstacle(Vector3 position, Vector3 halfExtents)
    : Entity(position, halfExtents) {}

void Obstacle::Draw() const {
    float sizeX = m_halfExtents.x * 2.0f;
    float sizeY = m_halfExtents.y * 2.0f;
    float sizeZ = m_halfExtents.z * 2.0f;

    DrawCube(m_position, sizeX, sizeY, sizeZ, DARKGRAY);
    DrawCubeWires(m_position, sizeX, sizeY, sizeZ, SKYBLUE); // contorno holográfico (raylib no define CYAN)
}
