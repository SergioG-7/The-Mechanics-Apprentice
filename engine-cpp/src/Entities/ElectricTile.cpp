#include "ElectricTile.h"
#include "../Core/Pulse.h"
#include "raylib.h"

ElectricTile::ElectricTile(Vector3 position, Vector3 size, float damage, float cycleInterval)
    : Entity(position, Vector3{ size.x * 0.5f, size.y * 0.5f, size.z * 0.5f }),
      m_damage(damage), m_cycleInterval(cycleInterval) {}

void ElectricTile::EnterState(ElectricTileState state) {
    m_state = state;
    m_stateTimer = 0.0f;
    // El flag se levanta AL ENTRAR en Descarga, no durante: así el golpe es
    // uno solo por ciclo por mucho que dure la animación del arco.
    if (state == ElectricTileState::Discharge) m_pendingDischarge = true;
}

void ElectricTile::Trigger() {
    // Solo desde Inactiva Y pasada la recuperación: si no, quedarse encima
    // la rearmaría en el mismo frame en que termina de descargar y sería un
    // Hazard por tick con otro nombre.
    if (m_state != ElectricTileState::Idle) return;
    if (m_stateTimer < kRecoveryDuration) return;
    EnterState(ElectricTileState::Warning);
}

void ElectricTile::Update(float dt) {
    m_stateTimer += dt;

    switch (m_state) {
        case ElectricTileState::Idle:
            // Baldosa de ciclo: se arma sola. El >= kRecoveryDuration de
            // Trigger no aplica aquí porque el propio ciclo ya es más largo.
            if (m_cycleInterval > 0.0f && m_stateTimer >= m_cycleInterval) {
                EnterState(ElectricTileState::Warning);
            }
            break;

        case ElectricTileState::Warning:
            if (m_stateTimer >= kWarningDuration) EnterState(ElectricTileState::Discharge);
            break;

        case ElectricTileState::Discharge:
            if (m_stateTimer >= kDischargeDuration) EnterState(ElectricTileState::Idle);
            break;
    }
}

bool ElectricTile::ConsumeDischarge() {
    if (!m_pendingDischarge) return false;
    m_pendingDischarge = false;
    return true;
}

void ElectricTile::Draw() const {
    float sizeX = m_halfExtents.x * 2.0f;
    float sizeY = m_halfExtents.y * 2.0f;
    float sizeZ = m_halfExtents.z * 2.0f;

    Color plate;
    Color border;

    switch (m_state) {
        case ElectricTileState::Idle:
            // Apagada y fría: se distingue de un Hazard (naranja óxido, daña
            // siempre) justo en que no parece una amenaza activa.
            plate = Color{ 45, 55, 75, 255 };
            border = Color{ 90, 110, 150, 255 };
            break;

        case ElectricTileState::Warning: {
            // Parpadeo que se acelera con la cuenta atrás, igual que el
            // Kamikaze antes de detonar: el ritmo ES la barra de progreso.
            bool on = Pulse::AcceleratingBlink(m_stateTimer, Pulse::Progress01(m_stateTimer, kWarningDuration), 0.32f, 0.06f);
            plate = on ? Color{ 240, 200, 60, 255 } : Color{ 120, 95, 30, 255 };
            border = YELLOW;
            break;
        }

        case ElectricTileState::Discharge:
        default:
            plate = Color{ 180, 235, 255, 255 };
            border = RAYWHITE;
            break;
    }

    DrawCube(m_position, sizeX, sizeY, sizeZ, plate);
    DrawCubeWires(m_position, sizeX, sizeY, sizeZ, border);

    if (m_state != ElectricTileState::Discharge) return;

    // Arco eléctrico: una zigzag entre esquinas opuestas, más un pilar de luz
    // corto. Suficiente para que se lea "esto acaba de soltar la descarga"
    // sin necesitar partículas ni un shader propio.
    constexpr int kArcSegments = 6;
    float topY = m_position.y + m_halfExtents.y + 0.02f;
    Vector3 previous{ m_position.x - m_halfExtents.x, topY, m_position.z - m_halfExtents.z };
    for (int i = 1; i <= kArcSegments; i++) {
        float t = static_cast<float>(i) / kArcSegments;
        float jitter = (i % 2 == 0) ? 0.25f : -0.25f;
        Vector3 next{
            m_position.x - m_halfExtents.x + sizeX * t,
            topY + 0.35f + jitter * 0.5f,
            m_position.z - m_halfExtents.z + sizeZ * t + jitter
        };
        DrawLine3D(previous, next, SKYBLUE);
        previous = next;
    }

    DrawCubeWires(Vector3{ m_position.x, topY + 0.5f, m_position.z }, sizeX * 0.6f, 1.0f, sizeZ * 0.6f,
                  Fade(SKYBLUE, 0.6f));
}
