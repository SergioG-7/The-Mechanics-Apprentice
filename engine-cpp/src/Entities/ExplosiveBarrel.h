#pragma once
#include "Actor.h"

// Objeto interactivo con HP: recibe daño del Player (ver
// CombatSystem::ResolveMeleeAttackOnBarrels) y, al llegar a 0, se marca
// "explotado". Application es quien, al ver HasExploded(), aplica el área
// (CombatSystem::ApplyAreaDamage, la misma que usa Enemy para el Kamikaze),
// dispara las chispas y lo retira del nivel -- el barril en sí no conoce ni
// al Player ni a los demás enemigos, igual que Enemy no conoce el resto del
// nivel más allá de su propia hitbox.
class ExplosiveBarrel : public Actor {
public:
    explicit ExplosiveBarrel(Vector3 position, float maxHP = 30.0f);

    void Update(float) override {}
    void Draw() const override;
    void TakeDamage(float amount, Vector3 knockbackDir) override;

    bool HasExploded() const { return m_exploded; }

    static constexpr float kExplosionRadius = 3.5f;
    static constexpr float kExplosionDamage = 50.0f;

private:
    bool m_exploded = false;
};
