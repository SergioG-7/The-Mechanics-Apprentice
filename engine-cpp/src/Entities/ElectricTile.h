#pragma once
#include "Entity.h"

// Ciclo de una baldosa eléctrica: inactiva, avisando, y descargando.
enum class ElectricTileState { Idle, Warning, Discharge };

// Trampa de suelo que avisa y luego suelta un golpe a quien siga encima, sin bloquear el paso.
class ElectricTile : public Entity {
public:
    // cycleInterval > 0: se arma sola cada tantos segundos. <= 0: solo se arma al pisarla.
    ElectricTile(Vector3 position, Vector3 size, float damage, float cycleInterval);

    void Update(float dt) override;
    void Draw() const override;

    // Arranca el aviso si la baldosa está inactiva y ya recuperada.
    void Trigger();

    // Devuelve true una sola vez, en el frame en que empieza a descargar.
    bool ConsumeDischarge();

    float GetDamage() const { return m_damage; }
    ElectricTileState GetState() const { return m_state; }

    static constexpr float kWarningDuration = 2.0f;   // ventana para salirse
    static constexpr float kDischargeDuration = 0.35f; // cuánto se ve el arco
    static constexpr float kRecoveryDuration = 1.5f;   // inactiva y desarmada tras descargar

private:
    float m_damage;
    float m_cycleInterval;
    ElectricTileState m_state = ElectricTileState::Idle;
    float m_stateTimer = 0.0f;
    bool m_pendingDischarge = false;

    void EnterState(ElectricTileState state);
};
