#include "Door.h"
#include "raylib.h"

Door::Door(Vector3 position, Vector3 halfExtents)
    : Entity(position, halfExtents) {}

void Door::Draw() const {
    float sizeX = m_halfExtents.x * 2.0f;
    float sizeZ = m_halfExtents.z * 2.0f;

    // Base plana en el suelo, apoyada en la parte baja del AABB del Door.
    constexpr float kBaseHeight = 0.15f;
    float baseY = m_position.y - m_halfExtents.y + kBaseHeight * 0.5f;
    Vector3 baseCenter{ m_position.x, baseY, m_position.z };
    DrawCube(baseCenter, sizeX, kBaseHeight, sizeZ, DARKGRAY);
    DrawCubeWires(baseCenter, sizeX, kBaseHeight, sizeZ, GRAY);

    // Haz de luz semi-transparente que marca la zona de extracción, elevándose
    // desde la base.
    constexpr float kBeamHeight = 3.0f;
    float beamY = m_position.y - m_halfExtents.y + kBaseHeight + kBeamHeight * 0.5f;
    Vector3 beamCenter{ m_position.x, beamY, m_position.z };
    Color beamColor{ 0, 228, 48, 120 };
    DrawCube(beamCenter, sizeX * 0.6f, kBeamHeight, sizeZ * 0.6f, beamColor);
    DrawCubeWires(beamCenter, sizeX * 0.6f, kBeamHeight, sizeZ * 0.6f, GREEN);
}
