#include "Hazard.h"
#include "raylib.h"

Hazard::Hazard(Vector3 position, Vector3 size, float damagePerTick)
    : Entity(position, Vector3{ size.x * 0.5f, size.y * 0.5f, size.z * 0.5f }), m_damagePerTick(damagePerTick) {}

void Hazard::Update(float dt) {
    m_tickTimer += dt;
}

bool Hazard::ConsumeTick() {
    if (m_tickTimer < kTickInterval) return false;
    m_tickTimer -= kTickInterval;
    return true;
}

void Hazard::Draw() const {
    float sizeX = m_halfExtents.x * 2.0f;
    float sizeY = m_halfExtents.y * 2.0f;
    float sizeZ = m_halfExtents.z * 2.0f;

    // Placa de color óxido con pinchos encima, para que se lea como peligro.
    Color plateColor{ 200, 90, 20, 255 };
    DrawCube(m_position, sizeX, sizeY, sizeZ, plateColor);
    DrawCubeWires(m_position, sizeX, sizeY, sizeZ, ORANGE);

    constexpr int kSpikesPerSide = 3;
    float spikeHeight = 0.35f;
    float spikeRadius = 0.08f;
    float topY = m_position.y + m_halfExtents.y;

    for (int ix = 0; ix < kSpikesPerSide; ix++) {
        for (int iz = 0; iz < kSpikesPerSide; iz++) {
            float fx = (kSpikesPerSide == 1) ? 0.5f : static_cast<float>(ix) / (kSpikesPerSide - 1);
            float fz = (kSpikesPerSide == 1) ? 0.5f : static_cast<float>(iz) / (kSpikesPerSide - 1);
            Vector3 spikeBase{
                m_position.x - m_halfExtents.x + fx * sizeX,
                topY,
                m_position.z - m_halfExtents.z + fz * sizeZ
            };
            DrawCylinder(spikeBase, spikeRadius, 0.0f, spikeHeight, 6, LIGHTGRAY);
        }
    }
}
