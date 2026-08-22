#pragma once
#include "Actor.h"

// Barril explosivo: recibe daño y, al quedarse a 0 HP, explota causando daño en área.
class ExplosiveBarrel : public Actor {
public:
    explicit ExplosiveBarrel(Vector3 position, float maxHP = 30.0f);
    ~ExplosiveBarrel();

    void Update(float) override {}
    void Draw() const override;
    void TakeDamage(float amount, Vector3 knockbackDir) override;

    bool HasExploded() const { return m_exploded; }

    // Devuelve true solo la primera vez que se llama tras explotar.
    bool ConsumeExplosion();

    // Actualiza el volumen del sonido de explosión según los ajustes de audio.
    void RefreshSfxVolume();

    static constexpr float kExplosionRadius = 3.5f;
    static constexpr float kExplosionDamage = 50.0f;

private:
    bool m_exploded = false;
    bool m_explosionResolved = false; // ya se le aplicó el AoE; ver ConsumeExplosion
    Sound m_deathSound{};
};
