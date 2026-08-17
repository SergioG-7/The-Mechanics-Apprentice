#include "ParticleSystem.h"
#include <algorithm>

void ParticleSystem::Emit(Vector3 origin, int count) {
    for (int i = 0; i < count; i++) {
        Particle p;
        p.position = origin;

        // Estallido hacia arriba y a los lados, magnitud aleatoria por eje
        // (GetRandomValue solo trabaja con enteros, de ahí el /100.0f).
        p.velocity = Vector3{
            static_cast<float>(GetRandomValue(-200, 200)) / 100.0f,
            static_cast<float>(GetRandomValue(150, 300)) / 100.0f,
            static_cast<float>(GetRandomValue(-200, 200)) / 100.0f
        };

        p.maxLifetime = static_cast<float>(GetRandomValue(20, 40)) / 100.0f; // 0.2 - 0.4s
        p.lifetime = p.maxLifetime;
        p.color = GetRandomValue(0, 1) == 0 ? YELLOW : WHITE;

        m_particles.push_back(p);
    }
}

void ParticleSystem::Update(float dt) {
    for (auto& p : m_particles) {
        p.velocity.y += kGravity * dt;
        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;
        p.position.z += p.velocity.z * dt;
        p.lifetime -= dt;
    }

    m_particles.erase(
        std::remove_if(m_particles.begin(), m_particles.end(),
                        [](const Particle& p) { return p.lifetime <= 0.0f; }),
        m_particles.end());
}

void ParticleSystem::Draw() const {
    for (const auto& p : m_particles) {
        float alphaRatio = p.lifetime / p.maxLifetime;
        DrawCube(p.position, kSize, kSize, kSize, Fade(p.color, alphaRatio));
    }
}
