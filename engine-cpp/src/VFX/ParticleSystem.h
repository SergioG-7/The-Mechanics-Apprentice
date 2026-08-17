#pragma once
#include "raylib.h"
#include <vector>

// Chispas de impacto: partículas efímeras con caída simple por gravedad, sin
// colisión contra el escenario. Application la posee, la actualiza y la
// dibuja cada frame igual que al resto de entidades del nivel.
class ParticleSystem {
public:
    void Emit(Vector3 origin, int count);
    void Update(float dt);
    void Draw() const;

private:
    struct Particle {
        Vector3 position{};
        Vector3 velocity{};
        float lifetime = 0.0f;
        float maxLifetime = 0.0f;
        Color color = WHITE;
    };

    static constexpr float kGravity = -9.8f;
    static constexpr float kSize = 0.1f;

    std::vector<Particle> m_particles;
};
