#pragma once
#include "Entity.h"

// Inactiva -> Aviso -> Descarga -> Inactiva. El aviso es la ventana en la que
// el jugador (y la IA, que no lo sabe) todavía puede salirse.
enum class ElectricTileState { Idle, Warning, Discharge };

// Trampa de suelo que, a diferencia de Hazard, NO daña por tick continuo:
// avisa y luego suelta UN golpe a todo lo que siga encima, amigo o enemigo.
// Eso la convierte en una herramienta táctica (se puede cebar a un zombi
// sobre ella) y no solo en un obstáculo a evitar.
//
// Tampoco bloquea el paso: vive en su propia lista (LevelData::electricTiles),
// nunca en 'obstacles', igual que Hazard.
class ElectricTile : public Entity {
public:
    // cycleInterval > 0: se arma sola cada tantos segundos, esté quien esté
    // encima (baldosa "de ciclo", útil para cortar un pasillo a intervalos).
    // cycleInterval <= 0: solo se arma cuando alguien la pisa (ver Trigger).
    ElectricTile(Vector3 position, Vector3 size, float damage, float cycleInterval);

    void Update(float dt) override;
    void Draw() const override;

    // Arranca el aviso si está inactiva y no en periodo de recuperación. La
    // llama CombatSystem al detectar a alguien encima; llamarla en cualquier
    // otro estado no hace nada, así que es segura de invocar cada frame.
    void Trigger();

    // true EXACTAMENTE en el frame en que entra en Descarga -- patrón
    // "consumir", igual que Enemy::ConsumeExplosionTrigger. Quien la llama
    // aplica el daño a todo lo que solape GetBoundingBox().
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
