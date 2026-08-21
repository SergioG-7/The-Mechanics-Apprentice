#include "ExplosiveBarrel.h"
#include "../Core/AudioSettings.h"
#include "raylib.h"

ExplosiveBarrel::ExplosiveBarrel(Vector3 position, float maxHP)
    : Actor(position, maxHP, Vector3{ 0.45f, 0.6f, 0.45f }) {
    // Mismo sonido que Enemy::m_deathSound -- "muerte de enemigo/barril"
    // comparte un único archivo, no hay uno específico de explosión.
    m_deathSound = LoadSound("assets/audio/sfx/dead_zombie.ogg");
    RefreshSfxVolume();
}

ExplosiveBarrel::~ExplosiveBarrel() {
    if (m_deathSound.frameCount > 0) UnloadSound(m_deathSound);
}

void ExplosiveBarrel::RefreshSfxVolume() {
    if (m_deathSound.frameCount > 0) SetSoundVolume(m_deathSound, AudioSettings::GetSfxVolume());
}

void ExplosiveBarrel::Draw() const {
    Vector3 base{ m_position.x, m_position.y - m_halfExtents.y, m_position.z };
    float height = m_halfExtents.y * 2.0f;
    DrawCylinder(base, m_halfExtents.x, m_halfExtents.x, height, 12, RED);
    DrawCylinderWires(base, m_halfExtents.x, m_halfExtents.x, height, 12, MAROON);
}

bool ExplosiveBarrel::ConsumeExplosion() {
    if (!m_exploded || m_explosionResolved) return false;
    m_explosionResolved = true;
    return true;
}

void ExplosiveBarrel::TakeDamage(float amount, Vector3 knockbackDir) {
    if (m_exploded) return;
    Actor::TakeDamage(amount, knockbackDir);
    if (!IsAlive()) {
        m_exploded = true;
        if (m_deathSound.frameCount > 0) PlaySound(m_deathSound);
    }
}
